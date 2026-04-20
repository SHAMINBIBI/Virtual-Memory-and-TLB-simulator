#ifndef PAGETABLE_HPP
#define PAGETABLE_HPP

#include <unordered_map>
#include <deque>
#include <list>
#include <vector>
#include <cstdint>
#include <string>
#include <limits>
#include <algorithm>

struct PageTableEntry {
    uint32_t frameNumber;
    bool valid = false;
    bool dirty = false;
};

class PageTable {
private:
    std::unordered_map<uint32_t, PageTableEntry> table;   // VPN -> Entry

    std::string policy;
    uint32_t maxFrames;
    int shiftAmount;                                       // for VPN calculation in OPT

    // FIFO
    std::deque<uint32_t> fifoQueue;

    // O(1) LRU
    std::list<uint32_t> lruOrder;                          // front = MRU, back = LRU
    std::unordered_map<uint32_t, std::list<uint32_t>::iterator> lruPosition;

    // Clairvoyant OPT
    bool isOPT = false;
    std::unordered_map<uint32_t, std::vector<size_t>> pageAccessTimes; // VPN → list of trace indices
    std::unordered_map<uint32_t, size_t> optNextIndex;               // next access index to consider
    const std::vector<std::pair<char, uint32_t>>* traceRef = nullptr;

    std::deque<uint32_t> freeFrames;                       // Proper Free Frame List

    uint32_t lastEvictedVPN = 0;
    bool lastVictimWasDirty = false;

    void moveToMRU(uint32_t vpn);                          // LRU helper

public:
    // Constructor
    PageTable(int frames, const std::string& pol, int shiftBits)
        : maxFrames(frames), policy(pol), shiftAmount(shiftBits) {
        // Initialize free frame list (deliverable #5)
        for (uint32_t i = 0; i < maxFrames; ++i) {
            freeFrames.push_back(i);
        }
    }

    // Call this ONCE before simulation if policy == "OPT"
    void precomputeOPT(const std::vector<std::pair<char, uint32_t>>& fullTrace);

    bool check(uint32_t vpn) const;
    uint32_t getFrame(uint32_t vpn) const;
    void setDirty(uint32_t vpn);

    // Core function: allocate a frame (handles eviction if needed)
    uint32_t allocateFrame(uint32_t newVPN, size_t currentTraceIdx = 0);

    bool isFull() const { return table.size() >= maxFrames; }
    bool victimWasDirty() const { return lastVictimWasDirty; }
    uint32_t getEvictedVPN() const { return lastEvictedVPN; }

    // For debugging / proof of correctness (10 instructions)
    void printState() const;
   void recordAccess(uint32_t vpn, size_t currentTraceIdx = 0);
};

#endif