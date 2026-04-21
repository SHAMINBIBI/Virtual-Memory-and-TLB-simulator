#ifndef TLB_HPP
#define TLB_HPP

#include <vector>
#include <cstdint>

struct TLBEntry {
    uint32_t vpn = 0;
    uint32_t frameNumber = 0;
    bool valid = false;
};

class TLB {
private:
    std::vector<TLBEntry> table;
    int capacity;

public:
    explicit TLB(int size) : capacity(size) {
        table.resize(size);
    }

    // Lookup TLB
    int lookup(uint32_t vpn) {
        for (const auto& entry : table) {
            if (entry.valid && entry.vpn == vpn) {
                return entry.frameNumber;
            }
        }
        return -1; // MISS
    }

    // Mandatory Invalidation on eviction
    void invalidate(uint32_t vpn) {
        for (auto& entry : table) {
            if (entry.vpn == vpn) {
                entry.valid = false;
                break;
            }
        }
    }

    // Update TLB (with round-robin replacement when full)
    void update(uint32_t vpn, uint32_t frame) {
        // Update if already exists
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
        // TLB full → round-robin
        static int nextReplace = 0;
        table[nextReplace].vpn = vpn;
        table[nextReplace].frameNumber = frame;
        table[nextReplace].valid = true;
        nextReplace = (nextReplace + 1) % capacity;
    }
};

#endif