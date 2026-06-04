// visual_driver.h
#pragma once
#include <string>
#include <atomic>

class VisualDriver {
private:
    std::atomic<bool> running{true};

public:
    VisualDriver(const std::string& cam_url);
    void run();
    void stop();
};
