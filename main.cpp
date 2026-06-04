// main.cpp
#include "farm_server.h"
#include <iostream>

int main(int argc, char** argv) {
    std::string ipcam_url  = "http://127.0.0.1:8080/shot.jpg";
    std::string model_path = "models/phi-3-vision.gguf";
    std::string server_url = "http://127.0.0.1:11434/v1";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--cam"    && i+1 < argc) ipcam_url  = argv[++i];
        else if (arg == "--model"  && i+1 < argc) model_path = argv[++i];
        else if (arg == "--url"    && i+1 < argc) server_url = argv[++i];
    }

    std::cout << "\n🚜 AgenticPhone Farm Server Started\n";
    std::cout << "Model  : " << model_path << "\n";
    std::cout << "Server : " << server_url << "\n";
    std::cout << "Camera : " << ipcam_url  << "\n\n";

    agent_cpp::ModelConfig config;
    config.base_url    = server_url;
    config.model       = model_path;
    config.n_ctx       = 32768;
    config.temperature = 0.6;

    FarmServer server(config);
    server.start(ipcam_url);
    return 0;
}
