#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>
#include "sw_full_stack.h"

#define DEBUG 0
#define STORE 0


uint16_t customRound(float value) {
    if (value > 0) {
        return static_cast<uint16_t>(std::ceil(value));
    } else {
        return static_cast<uint16_t>(std::floor(value));
    }
}

// Function to read Matrix Market (MTX) file
COOMatrix readMTXFile(const std::string& file_path) {
    COOMatrix matrix;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + file_path);
    }

    std::string line;
    bool is_comment = true;

    // Read header and metadata
    while (is_comment && std::getline(file, line)) {
        if (line[0] != '%') {
            is_comment = false;
            std::istringstream iss(line);
            iss >> matrix.n_rows >> matrix.n_cols >> matrix.nnz;
        }
    }

    matrix.row_indices.reserve(matrix.nnz);
    matrix.col_indices.reserve(matrix.nnz);
    matrix.values.reserve(matrix.nnz);

    // Read COO data
    uint32_t row, col;
    float value;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> row >> col >> value;
        //matrix.row_indices.push_back(row - 1); // Convert to zero-based indexing
        //matrix.col_indices.push_back(col - 1); // Convert to zero-based indexing
        matrix.row_indices.push_back(row); // Convert to 1-based indexing
        matrix.col_indices.push_back(col); // Convert to 1-based indexing
        matrix.values.push_back(customRound(value));
    }

    file.close();
    std::cout << "Matrix loaded: " << matrix.n_rows << "x" << matrix.n_cols << " with " << matrix.nnz << " non-zero elements." << std::endl;
    return matrix;
}

// Function to read Matrix Market (MTX) file
// 정보만 반환하기 위해 구성
// COOMatrix 구조체의 .nnz, .n_rows, .n_cols에 정보를 저장
// 이 정보만 빠르게 읽어와 사용이 가능해짐
COOMatrixInfo readMTXFileInformation(const std::string& file_path) {
    COOMatrixInfo matrix;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + file_path);
    }

    std::string line;
    bool is_comment = true;

    // Read header and metadata
    while (is_comment && std::getline(file, line)) {
        if (line[0] != '%') {
            is_comment = false;
            std::istringstream iss(line);
            iss >> matrix.n_rows >> matrix.n_cols >> matrix.nnz;
        }
    }
    file.close();
    std::cout << "Matrix information loaded: " << matrix.n_rows << "x" << matrix.n_cols << " with " << matrix.nnz << " non-zero elements." << std::endl;
    return matrix;
}


//저장 된 sw optimization 결과를 불러오기 위한 코드(여기가 아닌 다른 곳에서 사용)
//dat 확장자로 구성 된 파일을 불러올 때 사용
std::vector<std::vector<re_aligned_dram_format>> loadResultFromFile(const std::string& filename, int num_BG) {
    std::ifstream inFile(filename, std::ios::binary);

    if (!inFile.is_open()) {
        std::cerr << "Failed to open file for loading: " << filename << std::endl;
        return {};
    }

    std::vector<std::vector<re_aligned_dram_format>> result;

    // Load the number of outer vectors
    uint32_t outerSize;
    inFile.read(reinterpret_cast<char*>(&outerSize), sizeof(uint32_t));

    result.resize(outerSize);

    for (uint32_t i = 0; i < outerSize; ++i) {
        // Load the size of each inner vector
        uint32_t innerSize;
        inFile.read(reinterpret_cast<char*>(&innerSize), sizeof(uint32_t));

        result[i].resize(innerSize);

        // Load each re_aligned_dram_format
        for (uint32_t j = 0; j < innerSize; ++j) {
            inFile.read(reinterpret_cast<char*>(&result[i][j]), sizeof(re_aligned_dram_format));
        }
    }

    inFile.close();
    std::cout << "Data successfully loaded from " << filename << std::endl;

    return result;
}