// farm_server.cpp
#include "farm_server.h"
#include "project_utils.h"
#include "bluetooth_utils.h"
#include "serial_robot.h"

FarmServer::FarmServer(const std::string& gguf_model_path) 
    : agent_cpp::Agent(gguf_model_path) {

    ProjectUtils::init_heartbeat();

    // Load fast vision detector
    detector = cv::dnn::readNet("yolov8n.onnx");
    detector.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    detector.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    register_farm_tools();
}

void FarmServer::start(const std::string& ipcam_url) {
    std::cout << "🚜 AgenticPhone Self-Contained Server Started\n";
    
    // Run vision in background thread
    vision_thread = std::thread(&FarmServer::vision_loop, this, ipcam_url);

    // Start the agent.cpp server / HTTP interface if available
    // (or run agent main loop here)
    this->run();   // Adjust based on exact agent.cpp API
}

void FarmServer::vision_loop(const std::string& ipcam_url) {
    IPCam cam(ipcam_url);
    SerialRobot robot("/dev/ttyUSB0", 115200);

    int frame_count = 0;
    while (running) {
        cv::Mat frame = cam.getFrame();
        if (frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        bool needs_vlm = process_frame(frame);

        if (needs_vlm) {
            // Let the agent (LLM) reason
            std::string result = this->chat("Farm scene analysis: Unknown person or complex situation. Decide action.");
            std::cout << "[Agent Reasoning]: " << result << std::endl;
        }

        frame_count++;
        if (frame_count % 300 == 0) {
            ProjectUtils::save_progress_checkpoint(...);
        }

        cv::imshow("AgenticPhone", frame);
        if (cv::waitKey(1) == 'q') break;
    }
}

bool FarmServer::process_frame(cv::Mat& frame) {
    // YOLO ONNX forward + post-processing here
    // Return true only when VLM is truly needed (unknown person, ambiguity)
    // For known cases (you/boss, clear wild animal) act directly via BLE/robot
    return false; // placeholder
}

void FarmServer::register_farm_tools() {
    // Register tools so the LLM inside the agent can call them
    // Example:
    // this->register_tool("herd_cattle", ...);
    // this->register_tool("deter_wildlife", ...);
}
