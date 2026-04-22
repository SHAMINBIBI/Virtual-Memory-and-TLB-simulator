#ifndef FREEFRAMEMANAGER_HPP
#define FREEFRAMEMANAGER_HPP

#include <deque>
#include <cstdint>
#include <iostream>
#include "SystemConfig.hpp"

class FreeFrameManager {
private:
    std::deque<uint32_t> freeFrames;   // Stack-like for quick allocation (Deliverable 5)

public:
    explicit FreeFrameManager(int numFrames) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(numFrames); ++i) {
            freeFrames.push_back(i);
        }
        //std::cout << "[FreeFrameManager] Initialized with " << numFrames << " free frames.\n";
    }

    // Get a free frame if available
    bool getFreeFrame(uint32_t& frame) {
        if (!freeFrames.empty()) {
            frame = freeFrames.front();
            freeFrames.pop_front();
            return true;
        }
        return false;
    }

    // Return a frame back to free list (used after eviction)
    void releaseFrame(uint32_t frame) {
        freeFrames.push_back(frame);
    }

    bool isEmpty() const {
        return freeFrames.empty();
    }

    size_t getFreeCount() const {
        return freeFrames.size();
    }
};

#endif