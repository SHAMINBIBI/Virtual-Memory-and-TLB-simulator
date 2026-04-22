#ifndef PAGETABLE_HPP
#define PAGETABLE_HPP

#include <unordered_map>
#include <cstdint>
#include <string>
#include "SystemConfig.hpp"
#include "FreeFrameManager.hpp"
#include "ReplacementAlgorithms.hpp"

struct PageTableEntry {
    uint32_t frameNumber = 0;
    bool valid = false;
    bool dirty = false;
};

class PageTable {
private:
    int maxFrames;
    std::unordered_map<uint32_t, PageTableEntry> table;   // VPN → PTE (sparse)

    FreeFrameManager* freeFrameManager;
    ReplacementAlgorithms* replacementAlgo;

    uint32_t lastEvictedVPN = 0;
    bool lastVictimWasDirty = false;

public:
    PageTable(SystemConfig& config);
    ~PageTable();

    bool check(uint32_t vpn) const;           // Page Table hit?
    uint32_t getFrame(uint32_t vpn) const;
    void setDirty(uint32_t vpn);

    // Core: Allocate frame (handles free frame or eviction)
    uint32_t allocateFrame(uint32_t newVPN, size_t currentTraceIdx);

    bool isFull() const;
    bool victimWasDirty() const { return lastVictimWasDirty; }
    uint32_t getEvictedVPN() const { return lastEvictedVPN; }

    void recordAccess(uint32_t vpn, size_t currentTraceIdx);
    
    // For OPT precomputation
    void precomputeOPT(const std::vector<std::pair<char, uint32_t>>& fullTrace, int shiftAmount);

    // For debugging / proof of correctness
    void printState() const;
};

#endif