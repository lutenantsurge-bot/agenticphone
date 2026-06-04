#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#ifdef WITH_BLE
#include <simpleble/SimpleBLE.h>

namespace BluetoothUtils {

inline bool send_ble_command(
    const std::string& target_mac,
    const std::string& service_uuid,
    const std::string& characteristic_uuid,
    const std::vector<uint8_t>& command_data,
    int timeout_ms = 5000
) {
    try {
        auto adapter_list = SimpleBLE::Adapter::get_adapters();
        if (adapter_list.empty()) {
            std::cerr << "[BLE] No Bluetooth adapter found\n";
            return false;
        }

        auto& adapter = adapter_list[0];
        adapter.scan_for(timeout_ms / 1000);
        adapter.scan_stop();

        for (auto& p : adapter.scan_get_results()) {
            if (p.address() == target_mac) {
                p.connect();
                p.write_command(service_uuid, characteristic_uuid, command_data);
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                p.disconnect();
                std::cout << "[BLE] Command sent to " << target_mac << "\n";
                return true;
            }
        }
        std::cerr << "[BLE] Device not found: " << target_mac << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[BLE] Error: " << e.what() << "\n";
    }
    return false;
}

inline bool send_ble_command_str(
    const std::string& target_mac,
    const std::string& service_uuid,
    const std::string& characteristic_uuid,
    const std::string& command
) {
    std::vector<uint8_t> data(command.begin(), command.end());
    return send_ble_command(target_mac, service_uuid, characteristic_uuid, data);
}

} // namespace BluetoothUtils

#else

// Stub when BLE not available
namespace BluetoothUtils {

inline bool send_ble_command_str(
    const std::string&,
    const std::string&,
    const std::string&,
    const std::string& command
) {
    std::cout << "[BLE stub] Would send: " << command << "\n";
    return false;
}

} // namespace BluetoothUtils

#endif // WITH_BLE
