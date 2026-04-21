#ifndef REPLACEMENTALGORITHMS_HPP
#define REPLACEMENTALGORITHMS_HPP

#include <deque>
#include <list>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <limits>
#include <string>
#include <iostream>
#include "SystemConfig.hpp"

class ReplacementAlgorithms {
private:
    std::string policy;

    // FIFO
    std::deque<uint32_t> fifoQueue;

    // O(1) LRU - Doubly Linked List + Hash Map (Deliverable 9)
    std::list<uint32_t> lruOrder;                                   // front = MRU, back = LRU
    std::unordered_map<uint32_t, std::list<uint32_t>::iterator> lruPosition;

    // OPT (Clairvoyant - Deliverable 10)
    bool isOPT = false;
    std::unordered_map<uint32_t, std::vector<size_t>> pageAccessTimes;
    std::unordered_map<uint32_t, size_t> optNextIndex;
    const std::vector<std::pair<char, uint32_t>>* traceRef = nullptr;

public:
    explicit ReplacementAlgorithms(const std::string& pol) : policy(pol) {}

    // Call once before simulation if policy == "OPT"
    void precomputeOPT(const std::vector<std::pair<char, uint32_t>>& fullTrace, int shiftAmount) {
        if (policy != "OPT") return;
        traceRef = &fullTrace;
        isOPT = true;
        pageAccessTimes.clear();
        for (size_t i = 0; i < fullTrace.size(); ++i) {
            uint32_t vpn = fullTrace[i].second >> shiftAmount;
            pageAccessTimes[vpn].push_back(i);
        }
        std::cout << "[OPT] Precomputed future accesses for " << pageAccessTimes.size() << " pages.\n";
    }

    // Record access (used for LRU and OPT)
    void recordAccess(uint32_t vpn, size_t currentTraceIdx) {
        if (policy == "LRU") {
            // Move to MRU (front)
            if (lruPosition.count(vpn)) {
                lruOrder.erase(lruPosition[vpn]);
            }
            lruOrder.push_front(vpn);
            lruPosition[vpn] = lruOrder.begin();
        }
        else if (policy == "OPT" && isOPT) {
            auto& idx = optNextIndex[vpn];
            auto& accesses = pageAccessTimes[vpn];
            while (idx < accesses.size() && accesses[idx] <= currentTraceIdx) {
                ++idx;
            }
        }
    }

    // Select victim when eviction is needed
    uint32_t selectVictim(const std::unordered_map<uint32_t, bool>& currentPages, 
                          size_t currentTraceIdx = 0) {
        uint32_t victim = 0;

        if (policy == "FIFO") {
            if (!fifoQueue.empty()) {
                victim = fifoQueue.front();
                fifoQueue.pop_front();
            }
        }
        else if (policy == "LRU") {
            if (!lruOrder.empty()) {
                victim = lruOrder.back();
                lruOrder.pop_back();
                lruPosition.erase(victim);
            }
        }
        else if (policy == "OPT" && isOPT) {
            size_t farthest = 0;
            victim = 0;
            for (const auto& [vpn, _] : currentPages) {
                auto it = pageAccessTimes.find(vpn);
                if (it == pageAccessTimes.end()) {
                    victim = vpn;           // Never used again → best victim
                    return victim;
                }
                auto& accesses = it->second;
                auto nextIt = std::lower_bound(accesses.begin() + optNextIndex[vpn], 
                                               accesses.end(), currentTraceIdx + 1);
                size_t nextUse = (nextIt != accesses.end()) ? *nextIt : std::numeric_limits<size_t>::max();
                if (nextUse > farthest) {
                    farthest = nextUse;
                    victim = vpn;
                }
            }
        }

        return victim;
    }

    // Add a newly loaded page to replacement structures
    void addPage(uint32_t vpn) {
        if (policy == "FIFO") {
            fifoQueue.push_back(vpn);
        }
        else if (policy == "LRU") {
            lruOrder.push_front(vpn);
            lruPosition[vpn] = lruOrder.begin();
        }
        else if (policy == "OPT") {
            optNextIndex[vpn] = 0;
        }
    }

    // For debugging
    void printPolicy() const {
        std::cout << "Replacement Policy: " << policy << std::endl;
    }
};

#endif