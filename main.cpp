#include <iostream>
#include <vector>
#include <fstream>
#include <utility>
#include "SystemConfig.hpp"
#include "AddressTranslator.hpp"
#include "PageTable.hpp"
#include "TLB.hpp"
#include "LatencyEngine.hpp"


int main() {
    SystemConfig config;
    config.load("config.json");

    TLB myTLB(config.tlbSize);
    StatisticsEngine stats(config.tlbLatencyNs, config.ramLatencyNs, config.diskLatencyNs);

    PageTable myPageTable(config);
    // OPT Precompute
    std::vector<std::pair<char, uint32_t>> fullTrace;
    if (config.replacementPolicy == "OPT") {
        std::ifstream tracePre("test_trace_100k.txt");
        char op; uint32_t addr;
        while (tracePre >> op >> std::hex >> addr) {
            fullTrace.emplace_back(op, addr);
        }
        myPageTable.precomputeOPT(fullTrace, config.shiftAmount);
    }

    std::ifstream traceFile("addresses.txt");
    if (!traceFile.is_open()) {
        std::cerr << "Error: Cannot open trace file\n";
        return 1;
    }

    char opType;
    uint32_t virtualAddr;
    size_t traceIdx = 0;
    bool bypassTLB = false; // Set to true to skip TLB lookup and go directly to the page table.

        while (traceFile >> opType >> std::hex >> virtualAddr) {
        auto [vpn, offset] = AddressTranslator::splitAddress(virtualAddr, config);

        if (!AddressTranslator::isValidAddress(vpn, config)) {
            return 1;
        }

        int frame = -1;

        if (!bypassTLB) {
            frame = myTLB.lookup(vpn);
        }

        if (!bypassTLB && frame != -1) {
            // TLB HIT
            stats.recordEvent(MemoryEvent::TLB_HIT);
        } else {
            if (myPageTable.check(vpn)) {
                // Page Table Hit
                if (!bypassTLB) {
                    stats.recordEvent(MemoryEvent::TLB_MISS_PT_HIT);
                } else {
                    stats.recordEvent(MemoryEvent::PT_HIT);
                }
                frame = myPageTable.getFrame(vpn);
                if (!bypassTLB) {
                    myTLB.update(vpn, frame);
                }
            } else {
                // ========== PAGE FAULT ==========
                if (!bypassTLB) {
                    stats.recordEvent(MemoryEvent::PAGE_FAULT);
                } else {
                    stats.recordEvent(MemoryEvent::PAGE_FAULT);
                }

                // Allocate frame (this will trigger eviction when full)
                frame = myPageTable.allocateFrame(vpn, traceIdx);

                if (!bypassTLB) {
                    myTLB.update(vpn, frame);
                }

                // Handle dirty writeback
                if (myPageTable.victimWasDirty()) {
                    stats.recordEvent(MemoryEvent::DIRTY_WRITEBACK);
                }

                // Invalidate TLB entry for evicted page (if any)
                if (!bypassTLB) {
                    uint32_t evicted = myPageTable.getEvictedVPN();
                    if (evicted != 0) {
                        myTLB.invalidate(evicted);
                    }
                }
            }
        }

        // Record access for LRU / OPT
        myPageTable.recordAccess(vpn, traceIdx);

        // Set dirty bit for Write operations
        if (opType == 'W' || opType == 'w') {
            myPageTable.setDirty(vpn);
        }

        ++traceIdx;
        // Print first 10 instructions for Rubric WP2
        if (traceIdx <= 10) {
            std::cout << "[Trace " << traceIdx << "] Addr: 0x" << std::hex << virtualAddr 
                      << " | VPN: " << std::dec << vpn 
                      << " | Offset: " << offset 
                      << " | Op: " << opType << "\n";
        }
    }

    stats.printReport();
    return 0;
}