// visual_driver.cpp
#include "visual_driver.h"
#include "ipcam.h"
#include "project_utils.h"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <thread>

VisualDriver::VisualDriver(const std::string& cam_url) {
    ProjectUtils::init_heartbeat();
    std::cout << "[VisualDriver] Starting with camera: " << cam_url << "\n";
}

void VisualDriver::run() {
    IPCamConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 8080;
    cfg.enabled = true;

    IPCam cam(cfg);

    while (running) {
        // Get raw JPEG bytes
        std::vector<unsigned char> jpeg_data = cam.grab_jpeg();

        if (jpeg_data.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Decode JPEG bytes into cv::Mat
        cv::Mat frame = cv::imdecode(jpeg_data, cv::IMREAD_COLOR);

        if (frame.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        cv::imshow("AgenticPhone", frame);
        if (cv::waitKey(1) == 'q') break;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void VisualDriver::stop() {
    running = false;
}
