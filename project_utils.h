// project_utils.h
#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace ProjectUtils {

    inline std::string current_time() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    inline void init_heartbeat() {
        std::ofstream file("heartbeat.md", std::ios::trunc);
        if (file.is_open()) {
            file << "# AgenticPhone Heartbeat\n\n";
            file << "**Last updated:** " << current_time() << "\n";
            file << "**Status:** Running on Termux\n";
            file << "**Mode:** Farm Security + Herding Assist\n";
            file.close();
            std::cout << "[INFO] heartbeat.md initialized\n";
        }
    }

    inline void save_progress_checkpoint(
        const std::string& what_done,
        const std::string& what_doing,
        const std::string& plan_to_finish,
        const std::string& task_name = "AgenticPhone Task"
    ) {
        std::ofstream file("todo.md", std::ios::app);
        if (!file.is_open()) return;

        file << "\n## =F0=9F=9A=A8 High Context Checkpoint - " << current_time() <<
"\n";
        file << "**Task:** " << task_name << "\n";
        file << "**Context Usage:** High (saved)\n\n";
        file << "### What Has Been Done:\n" << what_done << "\n\n";
        file << "### Currently Doing:\n" << what_doing << "\n\n";
        file << "### Plan to Finish:\n" << plan_to_finish << "\n\n";
        file << "---\n";
        file.close();
        std::cout << "[WARNING] Checkpoint saved to todo.md\n";
    }
}

