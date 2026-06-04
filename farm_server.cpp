#include "farm_server.h"
#include "project_utils.h"
#include "src/agent.h"
#include "src/model.h"
#include <iostream>
#include <chrono>

FarmServer::FarmServer(const agent_cpp::ModelConfig& config)
    : config_(config) {

    ProjectUtils::init_heartbeat();

    // Load YOLO ONNX for fast detection
    try {
        detector_ = cv::dnn::readNet("yolov8n.onnx");
        detector_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        detector_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "[FarmServer] YOLO model loaded\n";
    } catch (...) {
        std::cerr << "[FarmServer] Warning: YOLO model not found — vision disabled\n";
    }
}

FarmServer::~FarmServer() {
    stop();
}

void FarmServer::start(const std::string& ipcam_url) {
    std::cout << "[FarmServer] Starting farm agent...\n";
    std::cout << "  Server: " << config_.base_url << "\n";
    std::cout << "  Model:  " << config_.model    << "\n\n";

    vision_thread_ = std::thread(&FarmServer::vision_loop, this, ipcam_url);

    // Keep main thread alive
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void FarmServer::stop() {
    running_ = false;
    if (vision_thread_.joinable())
        vision_thread_.join();
}

void FarmServer::vision_loop(const std::string& ipcam_url) {
    // IPCam setup
    IPCamConfig cam_cfg;
    cam_cfg.host    = ipcam_url;
    cam_cfg.port    = 8080;
    cam_cfg.enabled = true;
    IPCam cam(cam_cfg);

    // Robot setup
    SerialConfig robot_cfg;
    robot_cfg.port     = "/dev/ttyUSB0";
    robot_cfg.baud     = 115200;
    robot_cfg.enabled  = true;
    SerialRobot robot(robot_cfg);

    cv::Mat frame;
    int frame_count = 0;
    auto last_checkpoint = std::chrono::steady_clock::now();

    std::cout << "[FarmServer] Vision loop started\n";

    while (running_) {
        try {
            auto jpeg = cam.grab_jpeg();
            if (jpeg.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Decode JPEG to Mat
            cv::Mat buf(1, (int)jpeg.size(), CV_8UC1, jpeg.data());
            frame = cv::imdecode(buf, cv::IMREAD_COLOR);
            if (frame.empty()) continue;

            // Resize for performance
            cv::resize(frame, frame, cv::Size(480, 480));

            bool needs_vlm = process_frame(frame);

            if (needs_vlm) {
                // Send to LLM via agent ModelConfig
                // Wire to model_.chat() here when ready
                std::cout << "[FarmServer] VLM trigger — complex scene\n";
            }

            frame_count++;

            // Checkpoint every 60 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_checkpoint).count() > 60) {
                ProjectUtils::save_progress_checkpoint(
                    std::to_string(frame_count) + " frames processed",
                    "Vision loop running",
                    "Continue monitoring",
                    "FarmServer"
                );
                last_checkpoint = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // ~10 FPS

        } catch (const std::exception& e) {
            std::cerr << "[FarmServer] Error: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }
}

bool FarmServer::process_frame(cv::Mat& frame) {
    if (detector_.empty()) return false;

    cv::Mat blob = cv::dnn::blobFromImage(
        frame, 1.0/255.0,
        cv::Size(640, 640),
        cv::Scalar(), true, false
    );
    detector_.setInput(blob);

    std::vector<cv::Mat> outputs;
    try {
        std::vector<std::string> out_names = detector_.getUnconnectedOutLayersNames();
        detector_.forward(outputs, out_names);
    } catch (...) {
        return false;
    }

    // TODO: parse detections and apply reflex rules
    // return true when VLM reasoning needed
    return false;
}
