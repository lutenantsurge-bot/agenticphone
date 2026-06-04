// main.cpp
#include "farm_server.h"

int main(int argc, char** argv) {
    std::string ipcam = (argc > 1) ? argv[1] : "http://127.0.0.1:8080/shot.jpg";
    std::string model = (argc > 2) ? argv[2] : "models/phi-3-vision.gguf";
/*

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    bool        sbot          = false;
    std::string sbot_port     = "/dev/ttyUSB0";
    int         sbot_baud     = 115200;
    std::string cam_host      = "192.168.1.100";
    int         cam_port      = 8080;
    std::string server_url    = "http://127.0.0.1:8080/v1";
    std::string model_name    = "default";
    std::string sd_model_path;
    int         n_ctx         = 32768;
    double      temperature   = 0.6;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if      (arg == "--sbot")                        sbot          = true;
        else if (arg == "--sbot-port"   && i+1 < argc)  sbot_port     = argv[++i];
        else if (arg == "--sbot-baud"   && i+1 < argc)  sbot_baud     = std::stoi(argv[++i]);
        else if (arg == "--cam"         && i+1 < argc)  cam_host      = argv[++i];
        else if (arg == "--cam-port"    && i+1 < argc)  cam_port      = std::stoi(argv[++i]);
        else if (arg == "--url"         && i+1 < argc)  server_url    = argv[++i];
        else if (arg == "--model"       && i+1 < argc)  model_name    = argv[++i];
        else if (arg == "--ctx"         && i+1 < argc)  n_ctx         = std::stoi(argv[++i]);
        else if (arg == "--temp"        && i+1 < argc)  temperature   = std::stod(argv[++i]);
        else if (arg == "--sd-model"    && i+1 < argc)  sd_model_path = argv[++i];
    }

    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout <<   "║     llama-box-agent  v0.1            ║\n";
    std::cout <<   "║  text · vision · audio · image gen   ║\n";
    std::cout <<   "╚══════════════════════════════════════╝\n\n";
    std::cout << "Server:  " << server_url  << "\n";
    std::cout << "Model:   " << model_name  << "\n";
    std::cout << "Context: " << n_ctx       << "\n";
    std::cout << "Temp:    " << temperature << "\n\n";

    // ── Stable diffusion init ─────────────────────────────────────────────
#ifdef WITH_SD
    if (!sd_model_path.empty()) {
        std::cout << "[SD] Loading: " << sd_model_path << "\n";
        g_sd_model_path = sd_model_path;

        sd_ctx_params_t ctx_params;
        sd_ctx_params_init(&ctx_params);
        ctx_params.model_path = g_sd_model_path.c_str();
        ctx_params.n_threads  = -1;

        g_sd_ctx = new_sd_ctx(&ctx_params);
        std::cout << (g_sd_ctx ? "[SD] Ready\n" : "[SD] Failed to load\n");
    }
#endif

    // ── Robot init ────────────────────────────────────────────────────────
#ifdef WITH_ROBOT
    std::unique_ptr<SerialRobot> robot;
    if (sbot) {
        SerialConfig scfg = {sbot_port, sbot_baud, true};
        robot = std::make_unique<SerialRobot>(scfg);
        if (!robot->open()) {
            std::cerr << "[robot] Failed, continuing without\n";
            robot.reset();
        } else {
            std::cout << "[robot] Ready at " << sbot_port << "\n";
            robot_tools::RCControlTool rc(*robot);
            g_rc_control = [rc](const std::string& a, int s, int ang) mutable {
                return rc.execute(a, s, ang);
            };
        }
    }
#endif

    // ── Camera init ───────────────────────────────────────────────────────
#ifdef WITH_IPCAM
    std::unique_ptr<IPCam> cam;
    if (sbot) {
        IPCamConfig ccfg = {cam_host, cam_port, true};
        cam = std::make_unique<IPCam>(ccfg);
        if (cam->is_available()) {
            std::cout << "[cam] Ready at " << cam_host << ":" << cam_port << "\n";
            robot_tools::WebcamCaptureTool wc(*cam);
            g_webcam_capture = [wc](const std::string& s) mutable {
                return wc.capture(s);
            };
        } else {
            std::cerr << "[cam] Not reachable\n";
        }
    }
#endif

    std::cout << "\nTools: file_search read_file write_file shell_exec";
#ifdef WITH_SD
    std::cout << " generate_image";
#endif
#ifdef WITH_ROBOT
    std::cout << " rc_control";
#endif
#ifdef WITH_IPCAM
    std::cout << " webcam_capture";
#endif
    std::cout << "\n\nType request, 'tools', 'help', or 'exit':\n> ";

    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "exit" || input == "quit") break;
        if (input.empty()) { std::cout << "> "; continue; }

        if (input == "tools") {
            std::cout << build_tools_json() << "\n> ";
            continue;
        }

        if (input == "help") {
            std::cout << "\n--url <url>        inference server (default: http://127.0.0.1:8080/v1)\n";
            std::cout << "--model <name>     model name\n";
            std::cout << "--sd-model <path>  stable diffusion model (.safetensors)\n";
            std::cout << "--sbot             enable robot serial\n";
            std::cout << "--sbot-port <p>    serial port (default: /dev/ttyUSB0)\n";
            std::cout << "--cam <host>       IP webcam host\n";
            std::cout << "\nDirect tool call: tool_name key=value key=value\n";
            std::cout << "  generate_image prompt=\"a red fox\" output_path=fox.png\n";
            std::cout << "  file_search query=main.cpp\n";
            std::cout << "  shell_exec command=ls\n\n> ";
            continue;
        }

        // Direct tool call: "tool_name key=val key=val"
        std::istringstream iss(input);
        std::string tool_name;
        iss >> tool_name;
        std::map<std::string, std::string> tool_args;
        std::string token;
        bool is_tool = false;
        while (iss >> token) {
            auto eq = token.find('=');
            if (eq != std::string::npos) {
                is_tool = true;
                tool_args[token.substr(0, eq)] = token.substr(eq + 1);
            }
        }

        if (is_tool) {
            std::cout << "\n[" << tool_name << "]\n";
            std::cout << dispatch_tool(tool_name, tool_args) << "\n\n> ";
            continue;
        }

        // Send to inference server
        std::cout << "\n[agent] -> " << server_url << "\n";
        std::cout << "(wire agent.cpp Model here)\n\n> ";
    }

    // Cleanup
#ifdef WITH_SD
    if (g_sd_ctx) {
        free_sd_ctx(g_sd_ctx);
        g_sd_ctx = nullptr;
    }
#endif

    std::cout << "Goodbye.\n";
    return 0;
}*/

    FarmServer server(model);
    server.start(ipcam);

    return 0;
}
