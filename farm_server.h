#pragma once
#include "src/agent.h"
#include "ipcam.h"
#include "serial_robot.h"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <atomic>
#include <thread>
#include <string>
#include <memory>

class FarmServer {
public:
    explicit FarmServer(const agent_cpp::ModelConfig& config);
    ~FarmServer();

    void start(const std::string& ipcam_url = "http://127.0.0.1:8080/shot.jpg");
    void stop();

private:
    void vision_loop(const std::string& ipcam_url);
    bool process_frame(cv::Mat& frame);

    agent_cpp::ModelConfig  config_;
    cv::dnn::Net            detector_;
    std::atomic<bool>       running_{true};
    std::thread             vision_thread_;

    // BLE MAC — update to your device
    std::string ble_mac_     = "AA:BB:CC:DD:EE:FF";
    std::string ble_service_ = "0000ffe0-0000-1000-8000-00805f9b34fb";
    std::string ble_char_    = "0000ffe1-0000-1000-8000-00805f9b34fb";
};
