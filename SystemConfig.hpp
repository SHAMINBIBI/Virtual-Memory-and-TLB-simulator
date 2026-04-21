#ifndef SYSTEMCONFIG_HPP
#define SYSTEMCONFIG_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <cmath>
#include "json.hpp"

using json = nlohmann::json;

struct SystemConfig {
    int ramSizeKB;
    int pageSizeBytes;
    int tlbSize;
    std::string replacementPolicy;

    int tlbLatencyNs;
    int ramLatencyNs;
    long long diskLatencyNs;

    int shiftAmount;           // for VPN = addr >> shiftAmount
    uint32_t offsetMask;
    int numFrames;

    void load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file: " << filename << std::endl;
            exit(1);
        }

        json j = json::parse(file);

        ramSizeKB       = j["ram_size_kb"];
        pageSizeBytes   = j["page_size_bytes"];
        tlbSize         = j["tlb_size"];
        replacementPolicy = j.value("replacement_policy", "FIFO");

        tlbLatencyNs = j["latencies"]["tlb_ns"];
        ramLatencyNs = j["latencies"]["ram_ns"];
        int diskMs   = j["latencies"]["disk_ms"];
        diskLatencyNs = static_cast<long long>(diskMs) * 1000000LL;

        shiftAmount = static_cast<int>(std::log2(pageSizeBytes));
        offsetMask  = pageSizeBytes - 1;
        numFrames   = (ramSizeKB * 1024) / pageSizeBytes;

        std::cout << "[Config Loaded] RAM: " << ramSizeKB << " KB, Page: " << pageSizeBytes 
                  << " B (" << numFrames << " frames), TLB: " << tlbSize 
                  << ", Policy: " << replacementPolicy << std::endl;
    }
};

#endif