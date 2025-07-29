#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <cstring>

#include "matmul.h"

int main() {
    float m_matA[16] = {
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };
    float m_matB[16] = {
        17, 18, 19, 20,
        21, 22, 23, 24,
        25, 26, 27, 28,
        29, 30, 31, 32
    };

    MakeVulkan comp(m_matA, m_matB, sizeof(m_matA), sizeof(m_matB));

    try { comp.run(); }
    catch (const std::exception& e) { std::cerr << e.what() << std::endl; }

    for (int i = 0; i < 16; ++i)
        std::cout << comp.m_result[i] << " ";

    return 0;
}