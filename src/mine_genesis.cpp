// mine_genesis.cpp - Utility to mine a valid genesis block nonce
// Compile with: g++ -o mine_genesis mine_genesis.cpp -I. -Icrypto/randomx/src -Lbuild/lib -lrandomx -lpthread -O3

#include <cstdint>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <chrono>

extern "C" {
#include "crypto/randomx/src/randomx.h"
}

// Live chain genesis seed: Hash("RNG Genesis Seed")
// Precomputed uint256 raw bytes: 7ab7582f07fce1d160661402a5263cef73c17c86aa1c81b0884ff84471b8df0b
const uint8_t GENESIS_SEED[32] = {
    0x7a, 0xb7, 0x58, 0x2f, 0x07, 0xfc, 0xe1, 0xd1,
    0x60, 0x66, 0x14, 0x02, 0xa5, 0x26, 0x3c, 0xef,
    0x73, 0xc1, 0x7c, 0x86, 0xaa, 0x1c, 0x81, 0xb0,
    0x88, 0x4f, 0xf8, 0x44, 0x71, 0xb8, 0xdf, 0x0b
};

// Block header structure (80 bytes)
struct BlockHeader {
    int32_t nVersion;           // 4 bytes
    uint8_t hashPrevBlock[32];  // 32 bytes
    uint8_t hashMerkleRoot[32]; // 32 bytes
    uint32_t nTime;             // 4 bytes
    uint32_t nBits;             // 4 bytes
    uint32_t nNonce;            // 4 bytes
};

// Convert compact nBits to 256-bit target
void compact_to_target(uint32_t nBits, uint8_t target[32]) {
    memset(target, 0, 32);
    int size = (nBits >> 24) & 0xFF;
    uint32_t word = nBits & 0x007FFFFF;

    if (size <= 3) {
        word >>= 8 * (3 - size);
        target[0] = word & 0xFF;
        target[1] = (word >> 8) & 0xFF;
        target[2] = (word >> 16) & 0xFF;
    } else {
        int offset = size - 3;
        target[offset] = word & 0xFF;
        target[offset + 1] = (word >> 8) & 0xFF;
        target[offset + 2] = (word >> 16) & 0xFF;
    }
}

// Compare hash against target (little-endian)
bool hash_below_target(const uint8_t hash[32], const uint8_t target[32]) {
    // Compare from most significant byte (index 31) to least
    for (int i = 31; i >= 0; i--) {
        if (hash[i] < target[i]) return true;
        if (hash[i] > target[i]) return false;
    }
    return true; // Equal
}

std::string to_hex(const uint8_t* data, size_t len) {
    std::stringstream ss;
    for (size_t i = 0; i < len; i++) {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)data[i];
    }
    return ss.str();
}

void hex_to_bytes(const char* hex, uint8_t* out, size_t len) {
    for (size_t i = 0; i < len; i++) {
        sscanf(hex + 2*i, "%2hhx", &out[i]);
    }
}

int main(int argc, char** argv) {
    // Default: regtest parameters
    uint32_t nBits = 0x207fffff;  // Very easy target for regtest
    uint32_t nTime = 1738195200;

    // Mainnet merkle root (computed from coinbase tx)
    const char* merkle_root_hex = "b713a92ad8104e5a1650d02f96df9cb18bd6a39a222829ba4e4b5e79e4de7232";

    if (argc > 1) {
        // Allow specifying nBits for mainnet mining
        nBits = strtoul(argv[1], nullptr, 16);
    }

    std::cout << "Mining genesis block nonce..." << std::endl;
    std::cout << "nBits: 0x" << std::hex << nBits << std::dec << std::endl;
    std::cout << "nTime: " << nTime << std::endl;
    std::cout << "Merkle root: " << merkle_root_hex << std::endl;

    // Compute target
    uint8_t target[32];
    compact_to_target(nBits, target);
    std::cout << "Target: " << to_hex(target, 32) << std::endl;

    // Initialize RandomX in light mode (faster startup)
    randomx_flags flags = randomx_get_flags();
    randomx_cache* cache = randomx_alloc_cache(flags | RANDOMX_FLAG_JIT);
    if (!cache) {
        cache = randomx_alloc_cache(flags);
    }
    if (!cache) {
        std::cerr << "Failed to allocate RandomX cache" << std::endl;
        return 1;
    }

    randomx_init_cache(cache, GENESIS_SEED, 32);

    randomx_vm* vm = randomx_create_vm(flags | RANDOMX_FLAG_JIT, cache, nullptr);
    if (!vm) {
        vm = randomx_create_vm(flags, cache, nullptr);
    }
    if (!vm) {
        std::cerr << "Failed to create RandomX VM" << std::endl;
        return 1;
    }

    std::cout << "RandomX initialized (light mode)" << std::endl;

    // Prepare block header
    BlockHeader header;
    header.nVersion = 0x20000000;
    memset(header.hashPrevBlock, 0, 32);
    hex_to_bytes(merkle_root_hex, header.hashMerkleRoot, 32);
    // Reverse merkle root (Bitcoin uses little-endian internally)
    for (int i = 0; i < 16; i++) {
        std::swap(header.hashMerkleRoot[i], header.hashMerkleRoot[31-i]);
    }
    header.nTime = nTime;
    header.nBits = nBits;
    header.nNonce = 0;

    uint8_t hash[32];
    auto start_time = std::chrono::steady_clock::now();
    uint64_t attempts = 0;

    while (true) {
        // Compute RandomX hash
        randomx_calculate_hash(vm, &header, 80, hash);
        attempts++;

        if (hash_below_target(hash, target)) {
            auto end_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            std::cout << "\n*** FOUND VALID NONCE! ***" << std::endl;
            std::cout << "Nonce: " << header.nNonce << " (0x" << std::hex << header.nNonce << std::dec << ")" << std::endl;
            std::cout << "Hash: " << to_hex(hash, 32) << std::endl;
            std::cout << "Attempts: " << attempts << std::endl;
            std::cout << "Time: " << elapsed << "ms" << std::endl;
            std::cout << "Hashrate: " << (attempts * 1000 / (elapsed + 1)) << " H/s" << std::endl;
            break;
        }

        header.nNonce++;

        if (header.nNonce % 1000 == 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed > 0) {
                std::cout << "\rNonce: " << header.nNonce << " (" << (attempts / elapsed) << " H/s)" << std::flush;
            }
        }

        if (header.nNonce == 0) {
            std::cerr << "\nExhausted nonce space!" << std::endl;
            break;
        }
    }

    randomx_destroy_vm(vm);
    randomx_release_cache(cache);

    return 0;
}
