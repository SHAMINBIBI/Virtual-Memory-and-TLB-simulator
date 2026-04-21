#ifndef LATENCYENGINE_HPP
#define LATENCYENGINE_HPP

#include <iostream>

enum class MemoryEvent {
    TLB_HIT,
    TLB_MISS_PT_HIT,
    PAGE_FAULT,
    DIRTY_WRITEBACK
};

class StatisticsEngine {
private:
    long long totalTimeNs = 0;
    long long totalAccesses = 0;
    long long tlbHits = 0;
    long long pageFaults = 0;
    long long dirtyWrites = 0;

    int tlbLat;
    int ramLat;
    long long diskLat;

public:
    StatisticsEngine(int t, int r, long long d) : tlbLat(t), ramLat(r), diskLat(d) {}

    void recordEvent(MemoryEvent event) {
        totalAccesses++;

        switch (event) {
            case MemoryEvent::TLB_HIT:
                tlbHits++;
                totalTimeNs += (tlbLat + ramLat);   // TLB hit + RAM access
                break;

            case MemoryEvent::TLB_MISS_PT_HIT:
                totalTimeNs += (tlbLat + ramLat);
                break;

            case MemoryEvent::PAGE_FAULT:
                pageFaults++;
                totalTimeNs += (tlbLat + ramLat + diskLat);   // TLB + RAM + Disk read
                break;

            case MemoryEvent::DIRTY_WRITEBACK:
                dirtyWrites++;
                totalTimeNs += diskLat;   // Extra cost for writing dirty page
                break;
        }
    }

    double calculateEAT() {
        return totalAccesses == 0 ? 0.0 : static_cast<double>(totalTimeNs) / totalAccesses;
    }

    void printReport() {
        std::cout << "\n--- PERFORMANCE REPORT ---\n";
        std::cout << "Total Accesses: " << totalAccesses << "\n";
        std::cout << "TLB Hits: " << tlbHits << "\n";
        std::cout << "Page Faults: " << pageFaults << "\n";
        std::cout << "Dirty Disk Writes: " << dirtyWrites << "\n";
        std::cout << "Effective Access Time (EAT): " << calculateEAT() << " ns\n";
        std::cout << "--------------------------\n";
    }

    long long getDirtyWrites() const { return dirtyWrites; }
};

#endif