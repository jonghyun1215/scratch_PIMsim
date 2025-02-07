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


#endif
