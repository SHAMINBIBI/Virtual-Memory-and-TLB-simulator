#include <iostream>
#include <vector>
#include <fstream>
#include <utility>        // for std::pair
#include "json.hpp"
#include "PageTable.hpp"
#include "TLB.hpp"
#include "LatencyEngine.hpp"

int main() {
    SystemConfig config;
    config.load("config.json"); 

    TLB myTLB(config.tlbSize); 
    StatisticsEngine stats(config.tlbLatency, config.ramLatency, config.diskLatencyNs);
    
    int totalFrames = (config.ramSize * 1024) / config.pageSize; 
    PageTable myPageTable(totalFrames, config.replacementPolicy, config.shiftAmount); 

    // === OPT PRECOMPUTE (only if policy is OPT) ===
    std::vector<std::pair<char, uint32_t>> fullTrace;
    if (config.replacementPolicy == "OPT") {
        std::ifstream tracePre("test_trace_100k.txt");
        if (!tracePre.is_open()) {
            std::cerr << "Error: Cannot open trace file for OPT precomputation\n";
            return 1;
        }
        char op; uint32_t addr;
        while (tracePre >> op >> std::hex >> addr) {
            fullTrace.emplace_back(op, addr);
        }
        myPageTable.precomputeOPT(fullTrace);
        std::cout << "[OPT] Precomputed " << fullTrace.size() << " references.\n";
    }
    // ===============================================

    std::ifstream traceFile("test_trace_100k.txt");
    if (!traceFile.is_open()) {
        std::cerr << "Error: Cannot open trace file for simulation\n";
        return 1;
    }

    char opType;
    uint32_t virtualAddr;
    size_t traceIdx = 0;

    // Precompute maxVPN once (outside loop for efficiency)
    uint32_t maxVPN = (1ULL << (32 - config.shiftAmount)) - 1;

    while (traceFile >> opType >> std::hex >> virtualAddr) {
        uint32_t vpn = virtualAddr >> config.shiftAmount;

        // ========== FIX #08: OUT-OF-BOUNDS VALIDATION ==========
        if (vpn > maxVPN) {
            std::cerr << "FATAL: Out of bounds address 0x" << std::hex << virtualAddr 
                      << " (VPN=" << vpn << " > maxVPN=" << maxVPN << ") at trace index " 
                      << std::dec << traceIdx << std::endl;
            return 1;
        }
        // =======================================================

        int frame = myTLB.lookup(vpn);

        if (frame != -1) {
            // TLB HIT
            stats.recordEvent(MemoryEvent::TLB_HIT);
            
            // FIX #02 & #03: Record access for LRU/OPT even on TLB hit
            myPageTable.recordAccess(vpn, traceIdx);
            
        } else {
            // TLB MISS
            bool isPageInRAM = myPageTable.check(vpn);

            if (isPageInRAM) {
                // Page table hit (page in RAM, but TLB miss)
                stats.recordEvent(MemoryEvent::TLB_MISS_PT_HIT);
                frame = myPageTable.getFrame(vpn);
                myTLB.update(vpn, frame);
                
                // FIX #02 & #03: Record access for LRU/OPT
                myPageTable.recordAccess(vpn, traceIdx);
                
            } else {
                // PAGE FAULT
                stats.recordEvent(MemoryEvent::PAGE_FAULT);
                
                bool wasFull = myPageTable.isFull();
                uint32_t assignedFrame = myPageTable.allocateFrame(vpn, traceIdx);
                
                // FIX #01: Update TLB after page fault
                myTLB.update(vpn, assignedFrame);
                
                // FIX #02 & #03: Record access for LRU/OPT
                myPageTable.recordAccess(vpn, traceIdx);
                
                // Handle dirty writeback if victim was dirty
                if (wasFull && myPageTable.victimWasDirty()) {
                    stats.recordEvent(MemoryEvent::DIRTY_WRITEBACK);
                }
                
                // Invalidate TLB for evicted page
                if (wasFull) {
                    uint32_t evictedVPN = myPageTable.getEvictedVPN();
                    myTLB.invalidate(evictedVPN);
                }
            }
        }
        
        // Set dirty bit on write operation
        if (opType == 'W' || opType == 'w') {
            myPageTable.setDirty(vpn);
        }
        
        ++traceIdx;
    }

    stats.printReport();
    
    // Optional: Print page table state for debugging
    // myPageTable.printState();
    
    return 0;
}