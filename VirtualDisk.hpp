#ifndef VIRTUALDISK_HPP
#define VIRTUALDISK_HPP

#include <iostream>

class VirtualDisk {
public:
    long long dirtyWrites = 0;

    long long performDirtyWriteback() {
        dirtyWrites++;
        //std::cout << "[VirtualDisk] Dirty page written back to disk.\n";   // temporary for debugging
        return 10000000LL; // 10 ms in ns
    }

    void reset() {
        dirtyWrites = 0;
    }
};

#endif