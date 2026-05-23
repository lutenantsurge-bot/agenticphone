#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <chrono>
#include <memory>
#include <cstdlib>

#ifdef WITH_ROBOT
#include "serial_robot.h"
#include "robot_tools.h"
#endif

#ifdef WITH_IPCAM
#include "ipcam.h"
#include "robot_tools.h"
#endif

#ifdef WITH_SD
#include "stable-diffusion.cpp/include/stable-diffusion.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stable-diffusion.cpp/thirdparty/stb_image_write.h"
#endif

// ============================================================================
// Globals
// ============================================================================
std::function<std::string(const std::string&, int, int)> g_rc_control;
std::function<std::string(const std::string&)>           g_webcam_capture;

#ifdef WITH_SD
static sd_ctx_t*   g_sd_ctx        = nullptr;
static std::string g_sd_model_path;
#endif

// ============================================================================
// File / shell tools
// ============================================================================
std::string file_search(const std::string& query, const std::string& path) {
    std::string results;
    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.path().filename().string().find(query) != std::string::npos)
                results += entry.path().string() + "\n";
        }
    } catch (const std::exception& e) {
        results = std::string("Error: ") + e.what();
    }
    return results.empty() ? "No results found." : results;
}

std::string read_file(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f) return "Error: cannot open " + filepath;
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

std::string write_file(const std::string& filepath, const std::string& content) {
    std::ofstream f(filepath);
    if (!f) return "Error: cannot write to " + filepath;
    f << content;
    return "Written: " + filepath;
}

std::string shell_exec(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "Error: popen failed";
    char buf[256];
    std::string result;
    while (fgets(buf, sizeof(buf), pipe) != nullptr)
        result += buf;
    pclose(pipe);
    return result.empty() ? "(no output)" : result;
}

// ============================================================================
// Stable diffusion tool
// ============================================================================
#ifdef WITH_SD
std::string sd_generate(const std::string& prompt,
                         const std::string& output_path,
                         int width, int height, int steps) {
    if (!g_sd_ctx)
        return "Error: SD not initialized. Pass --sd-model at startup.";

    sd_img_gen_params_t params;
    sd_img_gen_params_init(&params);
    params.prompt               = prompt.c_str();
    params.negative_prompt      = "";
    params.width                = width;
    params.height               = height;
    params.seed                 = 42;

    // sample_params fields — check your header for exact names
    params.sample_params.sample_steps = steps;
    params.sample_params.guidance.txt_cfg = 7.5f;

    sd_image_t* images = generate_image(g_sd_ctx, &params);
    if (!images || !images[0].data)
        return "Error: image generation failed";

    std::filesystem::path p(output_path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());

    stbi_write_png(output_path.c_str(),
                   (int)images[0].width,
                   (int)images[0].height,
                   (int)images[0].channel,
                   images[0].data, 0);

    free(images[0].data);
    free(images);

    return "Image saved: " + output_path +
           " (" + std::to_string(width) + "x" + std::to_string(height) + ")";
}
#endif

// ============================================================================
// Tool JSON — built at runtime so #ifdef works outside strings
// ============================================================================
std::string build_tools_json() {
    std::string t = R"([
  {
    "type": "function",
    "function": {
      "name": "file_search",
      "description": "Search for files by name",
      "parameters": {
        "type": "object",
        "properties": {
          "query": { "type": "string" },
          "path":  { "type": "string", "default": "." }
        },
        "required": ["query"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "read_file",
      "description": "Read file contents",
      "parameters": {
        "type": "object",
        "properties": {
          "filepath": { "type": "string" }
        },
        "required": ["filepath"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "write_file",
      "description": "Write content to file",
      "parameters": {
        "type": "object",
        "properties": {
          "filepath": { "type": "string" },
          "content":  { "type": "string" }
        },
        "required": ["filepath", "content"]
      }
    }
  },
  {
    "type": "function",
    "function": {
      "name": "shell_exec",
      "description": "Execute a shell command",
      "parameters": {
        "type": "object",
        "properties": {
          "command": { "type": "string" }
        },
        "required": ["command"]
      }
    }
  }
)";

#ifdef WITH_SD
    t += R"(  ,{
    "type": "function",
    "function": {
      "name": "generate_image",
      "description": "Generate image from text prompt using stable diffusion",
      "parameters": {
        "type": "object",
        "properties": {
          "prompt":      { "type": "string" },
          "output_path": { "type": "string", "default": "output.png" },
          "width":       { "type": "integer", "default": 512 },
          "height":      { "type": "integer", "default": 512 },
          "steps":       { "type": "integer", "default": 20 }
        },
        "required": ["prompt"]
      }
    }
  }
)";
#endif

#ifdef WITH_ROBOT
    t += R"(  ,{
    "type": "function",
    "function": {
      "name": "rc_control",
      "description": "Control RC car via serial",
      "parameters": {
        "type": "object",
        "properties": {
          "action": {
            "type": "string",
            "enum": ["forward","backward","left","right","stop","emergency_stop","arm"]
          },
          "speed": { "type": "integer", "default": 160 },
          "angle": { "type": "integer", "default": 90 }
        },
        "required": ["action"]
      }
    }
  }
)";
#endif

#ifdef WITH_IPCAM
    t += R"(  ,{
    "type": "function",
    "function": {
      "name": "webcam_capture",
      "description": "Capture image from IP Webcam",
      "parameters": {
        "type": "object",
        "properties": {
          "save_as": { "type": "string", "default": "frame.jpg" }
        }
      }
    }
  }
)";
#endif

    t += "]";
    return t;
}

// ============================================================================
// Tool dispatcher
// ============================================================================
std::string dispatch_tool(const std::string& name,
                           const std::map<std::string, std::string>& args) {
    if (name == "file_search")
        return file_search(
            args.count("query") ? args.at("query") : "",
            args.count("path")  ? args.at("path")  : ".");

    if (name == "read_file")
        return read_file(args.count("filepath") ? args.at("filepath") : "");

    if (name == "write_file")
        return write_file(
            args.count("filepath") ? args.at("filepath") : "",
            args.count("content")  ? args.at("content")  : "");

    if (name == "shell_exec")
        return shell_exec(args.count("command") ? args.at("command") : "");

#ifdef WITH_SD
    if (name == "generate_image")
        return sd_generate(
            args.count("prompt")      ? args.at("prompt")              : "",
            args.count("output_path") ? args.at("output_path")         : "output.png",
            args.count("width")       ? std::stoi(args.at("width"))    : 512,
            args.count("height")      ? std::stoi(args.at("height"))   : 512,
            args.count("steps")       ? std::stoi(args.at("steps"))    : 20);
#endif

#ifdef WITH_ROBOT
    if (name == "rc_control" && g_rc_control)
        return g_rc_control(
            args.count("action") ? args.at("action")             : "stop",
            args.count("speed")  ? std::stoi(args.at("speed"))   : 160,
            args.count("angle")  ? std::stoi(args.at("angle"))   : 90);
#endif

#ifdef WITH_IPCAM
    if (name == "webcam_capture" && g_webcam_capture)
        return g_webcam_capture(
            args.count("save_as") ? args.at("save_as") : "frame.jpg");
#endif

    return "Unknown tool: " + name;
}

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
}
