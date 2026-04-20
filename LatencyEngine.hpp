#ifndef LATENCYENGINE_HPP
#define LATENCYENGINE_HPP

using namespace std;

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

    // These come from your Config Parser
    int tlbLat, ramLat;
    long long diskLat;

public:
    StatisticsEngine(int t, int r, long long d) : tlbLat(t), ramLat(r), diskLat(d) {}

    void recordEvent(MemoryEvent event) {
        totalAccesses++;
        
        switch (event) {
            case MemoryEvent::TLB_HIT:
                tlbHits++;
                totalTimeNs += (tlbLat + ramLat);   // ← changed: TLB hit still needs RAM access
                break;

            case MemoryEvent::TLB_MISS_PT_HIT:
                // Still had to check TLB first, then RAM
                totalTimeNs += (tlbLat + ramLat); 
                break;

            case MemoryEvent::PAGE_FAULT:
                pageFaults++;
                // TLB check + PT check + Disk Fetch
                totalTimeNs += (tlbLat + ramLat + diskLat);
                break;

            case MemoryEvent::DIRTY_WRITEBACK:
                dirtyWrites++;
                // Extra penalty for writing modified page back to disk
                totalTimeNs += diskLat;
                break;
        }
    }

    double calculateEAT() {
        if (totalAccesses == 0) return 0;
        return (double)totalTimeNs / totalAccesses;
    }

    void printReport() {
        cout << endl <<  "--- PERFORMANCE REPORT ---" << endl;
        cout << "Total Accesses: " << totalAccesses << endl;
        cout << "TLB Hits: " << tlbHits << endl;
        cout << "Page Faults: " << pageFaults << endl;
        cout << "Dirty Disk Writes: " << dirtyWrites << endl;
        cout << "Effective Access Time (EAT): " << calculateEAT() << " ns" << endl;
        cout << "--------------------------" << endl;
    }
};

#endif