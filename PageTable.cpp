#include "PageTable.hpp"
#include <iostream>
int maxFrames;
PageTable::PageTable(SystemConfig& config) {
    freeFrameManager = new FreeFrameManager(config.numFrames);
    replacementAlgo = new ReplacementAlgorithms(config.replacementPolicy);
    maxFrames = config.numFrames;
    std::cout << "[PageTable] Initialized with " << config.numFrames << " frames.\n";
}

PageTable::~PageTable() {
    delete freeFrameManager;
    delete replacementAlgo;
}

void PageTable::precomputeOPT(const std::vector<std::pair<char, uint32_t>>& fullTrace, int shiftAmount) {
    replacementAlgo->precomputeOPT(fullTrace, shiftAmount);
}

bool PageTable::check(uint32_t vpn) const {
    auto it = table.find(vpn);
    return (it != table.end() && it->second.valid);
}

uint32_t PageTable::getFrame(uint32_t vpn) const {
    return table.at(vpn).frameNumber;
}

void PageTable::setDirty(uint32_t vpn) {
    auto it = table.find(vpn);
    if (it != table.end()) {
        it->second.dirty = true;
    }
}

uint32_t PageTable::allocateFrame(uint32_t newVPN, size_t currentTraceIdx) {
    uint32_t frame = 0;

    std::cout << "[DEBUG] allocateFrame called for VPN=" << std::hex << newVPN 
              << " at trace " << std::dec << currentTraceIdx << std::endl;

    if (freeFrameManager->getFreeFrame(frame)) {
        std::cout << "[DEBUG] Assigned free frame " << frame << std::endl;
    } else {
        // Eviction needed
        std::unordered_map<uint32_t, bool> currentPages;
        for (const auto& p : table) {
            if (p.second.valid) {
                currentPages[p.first] = true;
            }
        }

        uint32_t victimVPN = replacementAlgo->selectVictim(currentPages, currentTraceIdx);

        std::cout << "[DEBUG] Evicting victim VPN=" << std::hex << victimVPN << std::dec << std::endl;

        lastEvictedVPN = victimVPN;
        auto it = table.find(victimVPN);
        if (it != table.end()) {
            lastVictimWasDirty = it->second.dirty;
            frame = it->second.frameNumber;

            if (lastVictimWasDirty) {
                std::cout << "[DEBUG] Victim was DIRTY → will trigger writeback!\n";
            }

            table.erase(it);
        }
    }

    // Insert new page
    table[newVPN] = {frame, true, false};
    replacementAlgo->addPage(newVPN);

    return frame;
}
bool PageTable::isFull() const {
    return table.size() >= static_cast<size_t>(maxFrames);
}
void PageTable::recordAccess(uint32_t vpn, size_t currentTraceIdx) {
    replacementAlgo->recordAccess(vpn, currentTraceIdx);
}

void PageTable::printState() const {
    std::cout << "[PageTable] Current pages in RAM: " << table.size() << std::endl;
}