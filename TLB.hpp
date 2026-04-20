#include <iostream>
#include <fstream>
#include <cmath>
#include "json.hpp" 
#include <string>

using namespace std;

using json = nlohmann::json;

struct SystemConfig {
    int ramSize;
    int pageSize;
    int tlbSize;
    int tlbLatency;
    int ramLatency;
    long long diskLatencyNs;
    int shiftAmount;
    std::string replacementPolicy = "FIFO";   // ← your new field

    void load(const std::string& filename) {   // ← note: I also fixed const std::string&
        std::ifstream file(filename);
        json j = json::parse(file);            // ← CHANGED from "data" to "j"

        ramSize = j["ram_size_kb"];
        pageSize = j["page_size_bytes"];
        tlbSize = j["tlb_size"];
        
        tlbLatency = j["latencies"]["tlb_ns"];
        ramLatency = j["latencies"]["ram_ns"];
        
        int diskMs = j["latencies"]["disk_ms"];
        diskLatencyNs = diskMs * 1000000LL;

        shiftAmount = static_cast<int>(log2(pageSize));

        // New policy (safe now)
        if (j.contains("replacement_policy")) {
            replacementPolicy = j["replacement_policy"];
        }
    }
};

struct TLBEntry {
    uint32_t vpn;
    uint32_t frameNumber;
    bool valid = false;
};

class TLB {
private:
    vector<TLBEntry> table;
    int capacity;

public:
    TLB(int size) : capacity(size) {
        table.resize(size);
    }

    // Task: TLB Lookup [cite: 64, 203]
    int lookup(uint32_t vpn) {
        for (const auto& entry : table) {
            if ((entry.valid) && (entry.vpn == vpn)) 
                return entry.frameNumber;
        }
        return -1; // TLB MISS
    }

    // Task: Mandatory Invalidation 
    void invalidate(uint32_t vpn) {
        for (auto& entry : table) {
            if (entry.vpn == vpn) {
                entry.valid = false;
                break;
            }
        }
    }

    void update(uint32_t vpn, uint32_t frame) {
    // First, check if already exists (update)
    for (auto& entry : table) {
        if (entry.valid && entry.vpn == vpn) {
            entry.frameNumber = frame;
            return;
        }
    }
    // Find empty slot
    for (auto& entry : table) {
        if (!entry.valid) {
            entry.vpn = vpn;
            entry.frameNumber = frame;
            entry.valid = true;
            return;
        }
    }
    // TLB full → simple round-robin replacement (better than always index 0)
    static int nextReplace = 0;
    table[nextReplace].vpn = vpn;
    table[nextReplace].frameNumber = frame;
    table[nextReplace].valid = true;
    nextReplace = (nextReplace + 1) % capacity;
}
    
};