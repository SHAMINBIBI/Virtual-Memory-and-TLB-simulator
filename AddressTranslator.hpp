#ifndef ADDRESSTRANSLATOR_HPP
#define ADDRESSTRANSLATOR_HPP

#include <cstdint>
#include <iostream>
#include "SystemConfig.hpp"

class AddressTranslator {
public:
    // Extracts VPN and Offset from 32-bit virtual address using bitwise operations
    static std::pair<uint32_t, uint32_t> splitAddress(uint32_t virtualAddr, const SystemConfig& config) {
        uint32_t vpn    = virtualAddr >> config.shiftAmount;
        uint32_t offset = virtualAddr & config.offsetMask;

        // Optional debug print (uncomment when needed for proof of correctness)
        // std::cout << "Addr: 0x" << std::hex << virtualAddr 
        //           << " → VPN: 0x" << vpn << " Offset: 0x" << offset << std::dec << std::endl;

        return {vpn, offset};
    }

    // Validates if VPN is within valid range for 32-bit address space
    static bool isValidAddress(uint32_t vpn, const SystemConfig& config) {
        uint32_t maxVPN = (1ULL << (32 - config.shiftAmount)) - 1;
        if (vpn > maxVPN) {
            std::cerr << "FATAL: Out of bounds VPN=" << std::hex << vpn 
                      << " (maxVPN=" << maxVPN << ")" << std::dec << std::endl;
            return false;
        }
        return true;
    }
};

#endif