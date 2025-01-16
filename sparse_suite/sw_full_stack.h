#ifndef SW_FULL_STACK_H
#define SW_FULL_STACK_H

#include <stdint.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <algorithm>


#define PARTITION_SIZE 16
#define GROUP_SIZE 7

typedef struct re_aligned_dram_format{
    uint32_t col_group[GROUP_SIZE]; //28B
    uint32_t row_buffer_empty; //4B
    uint16_t val[GROUP_SIZE * PARTITION_SIZE]; //32B x 7 = 224B
    uint32_t row[GROUP_SIZE * PARTITION_SIZE]; //32B x 14 = 448B
    uint16_t result[GROUP_SIZE * PARTITION_SIZE]; //32B x 7 = 224B
    uint16_t vec[GROUP_SIZE];
    uint16_t empty[41];
}re_aligned_dram_format;

// Structure to hold the COO matrix data
struct COOMatrix {
    std::vector<uint32_t> row_indices;
    std::vector<uint32_t> col_indices;
    std::vector<uint16_t> values;
    uint32_t n_rows;
    uint32_t n_cols;
    uint32_t nnz;
};

struct COOMatrixInfo {
    uint32_t n_rows;
    uint32_t n_cols;
    uint32_t nnz;
};

//std::vector<re_aligned_dram_format> spmv_format_transfer();
//std::vector<std::vector<re_aligned_dram_format>> sw_optimization();

// 에러가 나서 여기서는 함수 원형만 선언해주고
// 실제 함수 구현은 tilting_sparse_suite.cc에서 구현
// 아래 주석의 코드가 실제 함수 구현 코드
std::vector<std::vector<re_aligned_dram_format>>loadResultFromFile(const std::string& filename, int num_BG);
uint16_t customRound(float value);
COOMatrix readMTXFile(const std::string& file_path);
COOMatrixInfo readMTXFileInformation(const std::string& file_path);


/*uint16_t customRound(float value) {
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
        matrix.row_indices.push_back(row - 1); // Convert to zero-based indexing
        matrix.col_indices.push_back(col - 1); // Convert to zero-based indexing
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
*/

#endif
