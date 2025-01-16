#include <vector>
#include <cstdint>
#include <cstring> // For memcpy
#include <cstdlib> // For malloc, free
#include <fstream> // For file I/O
#include <iostream> // For std::cerr and std::cout
#include "../../sparse_suite/sw_full_stack.h"

bool compareData(const std::vector<std::vector<re_aligned_dram_format>>& original, const uint8_t* flatData, size_t totalSize) {
    const uint8_t* currentPtr = flatData;
    for (const auto& bg : original) {
        for (const auto& element : bg) {
            if (std::memcmp(&element, currentPtr, sizeof(re_aligned_dram_format)) != 0) {
                return false;
            }
            currentPtr += sizeof(re_aligned_dram_format);
        }
    }
    return true;
}

int main() {
    // Load DRAF_BG
    std::vector<std::vector<re_aligned_dram_format>> DRAF_BG = loadResultFromFile("../../sparse_suite/tiled_draf.dat", 64);

    // Calculate total size needed for the DRAF array
    size_t totalSize = 0;
    for (const auto& bg : DRAF_BG) {
        totalSize += bg.size() * sizeof(re_aligned_dram_format);
    }

    // Allocate memory for DRAF using malloc
    uint8_t* DRAF = static_cast<uint8_t*>(malloc(totalSize));
    if (DRAF == nullptr) {
        // Handle allocation failure
        perror("Memory allocation failed");
        return EXIT_FAILURE;
    }

    uint8_t* currentPtr = DRAF; // Preserve base pointer

    // Flatten DRAF_BG into DRAF
    for (const auto& bg : DRAF_BG) {
        for (const auto& element : bg) {
            std::memcpy(currentPtr, &element, sizeof(re_aligned_dram_format));
            currentPtr += sizeof(re_aligned_dram_format);
        }
    }
    // DRAF now contains all elements of DRAF_BG in byte format

    // Verify that the data matches the original
    if (compareData(DRAF_BG, DRAF, totalSize)) {
        std::cout << "Verification successful: The data matches." << std::endl;
    } else {
        std::cerr << "Verification failed: The data does not match." << std::endl;
    }

    // Clean up
    free(DRAF);

    return 0;
}

