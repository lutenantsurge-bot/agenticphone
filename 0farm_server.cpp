#include "farm_server.h"
#include "project_utils.h"
#include "bluetooth_utils.h"
#include "serial_robot.h"
#include <opencv2/dnn.hpp>
#include <iostream>

FarmServer::FarmServer(const std::string& gguf_path) 
    : agent_cpp::Agent(gguf_path) {
    
    ProjectUtils::init_heartbeat();
    
    try {
        detector = cv::dnn::readNet("yolov8n.onnx");
        detector.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        detector.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "[OK] YOLO ONNX model loaded\n";
    } catch (...) {
        std::cerr << "[ERROR] Failed to load ONNX model\n";
    }
}

FarmServer::~FarmServer() {
    stop();
}

void FarmServer::start(const std::string& ipcam_url) {
    std::cout << "🚜 AgenticPhone Self-Contained Farm Agent Started\n";
    vision_thread = std::thread(&FarmServer::vision_loop, this, ipcam_url);
    
    // Keep main thread alive for agent.cpp features
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void FarmServer::stop() {
    running = false;
    if (vision_thread.joinable()) vision_thread.join();
}

void FarmServer::vision_loop(const std::string& ipcam_url) {
    IPCam cam(ipcam_url);
    SerialRobot robot("/dev/ttyUSB0", 115200);   // Adjust port if needed
    
    cv::Mat frame;
    int frame_count = 0;
    auto last_heartbeat = std::chrono::steady_clock::now();

    while (running) {
        try {
            frame = cam.getFrame();
            if (frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // Resize for performance & lower memory
            cv::resize(frame, frame, cv::Size(480, 480));

            bool needs_vlm = process_frame(frame);

            if (needs_vlm) {
                std::string response = this->chat("Analyze farm scene. Give clear action: herd, deter, or alert.");
                std::cout << "[Agent] " << response << std::endl;
            }

            frame_count++;
            
            // Very light heartbeat
            if (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - last_heartbeat).count() > 60) {
                
                ProjectUtils::save_progress_checkpoint(
                    std::to_string(frame_count) + " frames processed",
                    "Stable vision loop running",
                    "Continue monitoring",
                    "Farm Agent"
                );
                last_heartbeat = std::chrono::steady_clock::now();
            }

            cv::imshow("AgenticPhone", frame);
            if (cv::waitKey(1) == 'q') break;

            std::this_thread::sleep_for(std::chrono::milliseconds(80)); // ~12 FPS max
        } 
        catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}

bool FarmServer::process_frame(cv::Mat& frame) {
    // TODO: Add your YOLO post-processing here
    // For stability, you can start with simple motion detection first
    // Only run full DNN every few frames

    // Example placeholder:
    // if (person_detected && is_known) → HERD via BLE
    // if (wild_animal) → DETER
    // if (unknown person or complex) → return true (VLM)

    return false;   // Change when you implement detection
}
~FarmServer(){asm("NOP");}
