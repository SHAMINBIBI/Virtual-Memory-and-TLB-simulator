#include "PageTable.hpp"
#include <iostream>

void PageTable::moveToMRU(uint32_t vpn) {
    if (lruPosition.count(vpn)) {
        lruOrder.erase(lruPosition[vpn]);
    }
    lruOrder.push_front(vpn);
    lruPosition[vpn] = lruOrder.begin();
}

void PageTable::precomputeOPT(const std::vector<std::pair<char, uint32_t>>& fullTrace) {
    traceRef = &fullTrace;
    isOPT = true;
    pageAccessTimes.clear();
    for (size_t i = 0; i < fullTrace.size(); ++i) {
        uint32_t vpn = fullTrace[i].second >> shiftAmount;
        pageAccessTimes[vpn].push_back(i);
    }
}

bool PageTable::check(uint32_t vpn) const {
    return table.count(vpn) && table.at(vpn).valid;
}

uint32_t PageTable::getFrame(uint32_t vpn) const {
    return table.at(vpn).frameNumber;
}

void PageTable::setDirty(uint32_t vpn) {
    if (table.count(vpn)) table[vpn].dirty = true;
}

uint32_t PageTable::allocateFrame(uint32_t newVPN, size_t currentTraceIdx) {
    uint32_t frame;

    if (!freeFrames.empty()) {
        frame = freeFrames.front();
        freeFrames.pop_front();
    } else {
        // Eviction needed
        uint32_t victimVPN = 0;
        if (policy == "FIFO") {
            victimVPN = fifoQueue.front(); fifoQueue.pop_front();
        } else if (policy == "LRU") {
            victimVPN = lruOrder.back();
            lruOrder.pop_back();
            lruPosition.erase(victimVPN);
        } else if (policy == "OPT" && isOPT) {
            // Clairvoyant OPT: find page with farthest future use
            size_t farthest = 0;
            for (const auto& [vpn, entry] : table) {
                if (!entry.valid) continue;
                auto& accesses = pageAccessTimes[vpn];
                auto it = std::lower_bound(accesses.begin() + optNextIndex[vpn], accesses.end(), currentTraceIdx + 1);
                size_t nextUse = (it != accesses.end()) ? *it : std::numeric_limits<size_t>::max();
                if (nextUse > farthest) {
                    farthest = nextUse;
                    victimVPN = vpn;
                }
            }
        }

        lastEvictedVPN = victimVPN;
        lastVictimWasDirty = table[victimVPN].dirty;
        frame = table[victimVPN].frameNumber;
        table.erase(victimVPN);
    }

    // Map new page
    table[newVPN] = {frame, true, false};

    // Update replacement structure
    if (policy == "FIFO") {
        fifoQueue.push_back(newVPN);
    } else if (policy == "LRU") {
        moveToMRU(newVPN);
    } else if (policy == "OPT") {
        optNextIndex[newVPN] = 0;   // will be updated on next access if needed
    }

    return frame;
}

void PageTable::printState() const {
    std::cout << "PageTable state (" << policy << "): " << table.size() << "/" << maxFrames << " pages in RAM\n";
    // You can expand this for the 10-instruction proof if needed
}
void PageTable::recordAccess(uint32_t vpn, size_t currentTraceIdx) {
    if (policy == "LRU") {
        moveToMRU(vpn);
    }
    if (policy == "OPT" && isOPT) {
        auto& idx = optNextIndex[vpn];
        auto& accesses = pageAccessTimes[vpn];
        // Advance to next future access index if current one is in the past
        while (idx < accesses.size() && accesses[idx] <= currentTraceIdx) {
            idx++;
        }
    }
}