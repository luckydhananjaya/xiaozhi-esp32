#pragma once

#include <string>
#include <vector>

struct KawanHidDevice {
    std::string name;
    std::string address;
    int rssi = 0;
    uint8_t addr_type = 0;
    uint8_t bda[6] = {};
};

namespace KawanHid {
bool Start();
void Stop();
bool Scanning();
bool Connected();
std::string Status();
std::vector<KawanHidDevice> Devices();
void Scan();
bool Connect(int index);
void Forget();
}  // namespace KawanHid
