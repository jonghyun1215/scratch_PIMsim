#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>
#include <tuple>
#include "sw_full_stack.h"

#define DEBUG 0
#define STORE 0

// Structure to hold the COO matrix data

 //HEADER 파일에 정의 되어 있음
/*struct COOMatrix {
    std::vector<uint32_t> row_indices;
    std::vector<uint32_t> col_indices;
    std::vector<uint16_t> values;
    uint32_t n_rows;
    uint32_t n_cols;
    uint32_t nnz;
};*/

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
        matrix.row_indices.push_back(row - 1); // Convert to zero-based indexing
        matrix.col_indices.push_back(col - 1); // Convert to zero-based indexing
        //matrix.row_indices.push_back(row); // Convert to 1-based indexing
        //matrix.col_indices.push_back(col); // Convert to 1-based indexing
        matrix.values.push_back(customRound(value));
    }

    file.close();
    if(DEBUG)
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
    if(DEBUG)
        std::cout << "Matrix information loaded: " << matrix.n_rows << "x" << matrix.n_cols << " with " << matrix.nnz << " non-zero elements." << std::endl;
    return matrix;
}

////////////////////////FORMAT TRANSFER////////////////////////
typedef struct align_row_buffer{
    uint16_t val[PARTITION_SIZE]; //actual type is fp16 not float
    uint32_t row[PARTITION_SIZE];
}align_row_buffer;

typedef struct align_dram_row{
    uint32_t col_group[GROUP_SIZE];
    uint32_t row_buffer_empty; //if 1KB -> uint32_t
    align_row_buffer row_buffer[GROUP_SIZE];
    uint16_t result[GROUP_SIZE * PARTITION_SIZE];
}align_dram_row;

// re_aligned_dram_format is the final format to store in DRAM
// re_aligned format is stored in header file
std::vector<re_aligned_dram_format> spmv_format_transfer(
    std::vector<uint32_t>& row_indices, std::vector<uint32_t>& col_indices, std::vector<uint16_t>& val, uint32_t& NEW_NNZ) 
{
    std::vector<align_row_buffer> row_buffer(NEW_NNZ);
    std::vector<uint32_t> col_index(NEW_NNZ, 0);
    uint32_t t = 0;
    uint32_t nnz_count = 0;

    // Initialize row_buffer
    for(auto& buffer : row_buffer) {
        for(uint32_t j = 0; j < PARTITION_SIZE; j++) {
            buffer.row[j] = 0;
            buffer.val[j] = 0;
        }
    }
    if(DEBUG) std::cout << "row buffer initialized" << std::endl;

    int index = 0;
    for(uint32_t i = 0; i < NEW_NNZ; i++) {
        uint32_t row = row_indices[i];
        uint32_t col = col_indices[i];
        uint16_t value = val[i];

        index = nnz_count % PARTITION_SIZE;
        if(col_index[t] != col && index != 0) {
            t++;
            nnz_count = 0;
            index = nnz_count % PARTITION_SIZE;
        }
        if(index == 0 && nnz_count != 0) {
            t++;
        }

        if (t >= col_index.size()) {
            col_index.resize(t + 1);
            row_buffer.resize(t + 1);
        }
        
        col_index[t] = col;
        row_buffer[t].row[index] = row;
        row_buffer[t].val[index] = value;
        nnz_count++;
    }
    if(DEBUG) std::cout << "row buffer filled" << std::endl;

    // Define dram_row vector
    std::vector<align_dram_row> dram_row((t / GROUP_SIZE) + 1);

    // Initialize dram_row
    for(auto& row : dram_row) {
        for(uint32_t j = 0; j < GROUP_SIZE; j++) {
            for(int k = 0; k < PARTITION_SIZE; k++) {
                row.row_buffer[j].row[k] = 0;
                row.row_buffer[j].val[k] = 0;
            }
            row.col_group[j] = 0;
        }
        row.row_buffer_empty = 0;
    }
    if(DEBUG) std::cout << "dram row initialized" << std::endl;

    // Fill dram_row
    for(size_t i = 0; i <= t; i++) {
        int row_index = i / GROUP_SIZE;
        int buffer_index = i % GROUP_SIZE;
        dram_row[row_index].row_buffer[buffer_index] = row_buffer[i];
        dram_row[row_index].col_group[buffer_index] = col_index[i];
    }
    if(DEBUG) std::cout << "dram row filled" << std::endl;

    // Define store_format vector
    std::vector<re_aligned_dram_format> store_format((t / GROUP_SIZE) + 1);

    // Fill store_format
    for(uint32_t i = 0; i <= t / GROUP_SIZE; i++) {
        for(uint32_t j = 0; j < GROUP_SIZE; j++) {
            for(uint32_t k = 0; k < PARTITION_SIZE; k++) {
                uint32_t buffer_index = j * PARTITION_SIZE + k;
                store_format[i].row[buffer_index] = dram_row[i].row_buffer[j].row[k];
                //store_format[i].val[buffer_index] = dram_row[i].row_buffer[j].val[k];
                
                // TW added
                // TEST를 위해 추가함 우선 모든 value 값을 1로 설정
                if(store_format[i].row[buffer_index] != 0)
                    store_format[i].val[buffer_index] = 1;
                else
                    store_format[i].val[buffer_index] = 0;
            }
            store_format[i].col_group[j] = dram_row[i].col_group[j];
            if(dram_row[i].col_group[j] != 0) {
                store_format[i].vec[j] = 1;
            }
            else {
                store_format[i].vec[j] = 0;
            }
            //store_format[i].vec[j] = vec[dram_row[i].col_group[j]]; // Assumes vec is defined elsewhere
        }
    }
    if(DEBUG) std::cout << "store format filled" << std::endl;

    return store_format;
}

////////////////////////FORMAT TRANSFER////////////////////////

//Function to convert COO matrix to CSR matrix
struct CSRMatrix {
    std::vector<uint32_t> row_ptr;
    std::vector<uint32_t> col_indices;
    std::vector<uint16_t> values;
    uint32_t n_rows;
    uint32_t n_cols;
    uint32_t nnz;
};

CSRMatrix coo_to_csr(const COOMatrix& coo) {
    CSRMatrix csr;
    csr.n_rows = coo.n_rows;
    csr.n_cols = coo.n_cols;
    csr.nnz = coo.nnz;

    // 1. COO 데이터 정렬 (row 우선, column 차선)
    std::vector<std::tuple<uint32_t, uint32_t, uint16_t>> entries;
    entries.reserve(coo.nnz);
    for (size_t i = 0; i < coo.nnz; ++i) {
        entries.emplace_back(coo.row_indices[i], coo.col_indices[i], coo.values[i]);
    }

    std::sort(entries.begin(), entries.end());

    // 2. CSR row_ptr 배열 생성
    csr.row_ptr.resize(coo.n_rows + 1, 0);
    for (const auto& entry : entries) {
        csr.row_ptr[std::get<0>(entry) + 1]++;
    }

    // 누적 합 계산
    for (uint32_t i = 1; i <= coo.n_rows; ++i) {
        csr.row_ptr[i] += csr.row_ptr[i - 1];
    }

    // 3. CSR column/value 데이터 채우기
    csr.col_indices.reserve(coo.nnz);
    csr.values.reserve(coo.nnz);
    for (const auto& entry : entries) {
        csr.col_indices.push_back(std::get<1>(entry));
        csr.values.push_back(std::get<2>(entry));
    }

    // 4. row_ptr 크기 최적화
    uint32_t last_non_empty_row = coo.n_rows;
    while (last_non_empty_row > 0 && csr.row_ptr[last_non_empty_row] == csr.row_ptr[last_non_empty_row - 1]) {
        last_non_empty_row--;
    }
    csr.row_ptr.resize(last_non_empty_row + 1);

    return csr;
}

//Function to convert COO matrix to CSC matrix
struct CSCMatrix {
    std::vector<uint32_t> col_ptr;
    std::vector<uint32_t> row_indices;
    std::vector<uint16_t> values;
    uint32_t n_rows;
    uint32_t n_cols;
    uint32_t nnz;
};

CSCMatrix coo_to_csc(const COOMatrix& coo) {
    CSCMatrix csc;
    csc.n_rows = coo.n_rows;
    csc.n_cols = coo.n_cols;
    csc.nnz = coo.nnz;

    // 1. col_ptr 초기화 (이미 정렬된 데이터 가정)
    csc.col_ptr.resize(coo.n_cols + 1, 0);
    for (uint32_t i = 0; i < coo.nnz; ++i) {
        csc.col_ptr[coo.col_indices[i] + 1]++;
    }

    // 2. 누적 합 계산 
    for (uint32_t i = 1; i <= coo.n_cols; ++i) {
        csc.col_ptr[i] += csc.col_ptr[i - 1];
    }

    // 3. 데이터 직접 복사 (이미 정렬된 상태)
    csc.row_indices = coo.row_indices;
    csc.values = coo.values;

    // 4. col_ptr 크기 최적화
    uint32_t last_non_zero_col = coo.n_cols;
    while (last_non_zero_col > 0 && csc.col_ptr[last_non_zero_col] == csc.col_ptr[last_non_zero_col - 1]) {
        last_non_zero_col--;
    }
    csc.col_ptr.resize(last_non_zero_col + 1);

    return csc;
}

// Main function
int main() {
    // 처리할 데이터셋 목록
    const std::vector<std::string> dataset_names = {
        "ASIC_100k", "bcsstk32", "cant", "consph", 
        "crankseg_2", "ct20stif", "G2_circuit", "lhr71",
        "ohne2", "pdb1HYS", "pwtk", "rma10",
        "shipsec1", "soc-sign-epinions", "sorted_consph",
        "Stanford", "webbase-1M", "xenon2"
    };

    const int num_tiles = 64;

    bool memory_usage = false;

    // 각 데이터셋별 처리
    for (const auto& dataset : dataset_names) {
        // 1. 파일 경로 설정
        const std::string base_path = "./suite/partitioned_default/" + dataset + "/partition_";
        const std::string output_path = "draf_dat/tiled_draf_" + dataset + ".dat";

        // 2. DRAF 저장 벡터 초기화
        std::vector<re_aligned_dram_format> draf_result(num_tiles);
        uint32_t max_size = 0;
        uint32_t max_index = 0;

        std::cout << "Processing dataset: " << dataset << std::endl;
        // If coo, csc format is stored, compare the memory usage
        // If draf format is stored, compare the memory usage
        std::string file_path = "./suite/sorted_suite/" + dataset + "_new.mtx";
        COOMatrix matrix = readMTXFile(file_path);
        //Assume matrix value is stored in 16-bit unsigned integer
        //index are stored in 32-bit unsigned integer
        std::cout << "COO Format Memory Usage: " << (matrix.nnz * 10) << "Byte used" << std::endl;
        //COO matrix to CSR matrix
        CSRMatrix csr_matrix = coo_to_csr(matrix);
        std::cout << "CSR Format Memory Usage: " << (csr_matrix.nnz * 6) + (csr_matrix.n_rows+1) * 4 << "Byte used" << std::endl;

        //COO matrix to CSC matrix
        CSCMatrix csc_matrix = coo_to_csc(matrix);
        std::cout << "CSC Format Memory Usage: " << (csc_matrix.nnz * 6) + (csc_matrix.n_cols+1) * 4 << "Byte used" << std::endl;
        
        int row_used = 0;

        // 3. 타일별 처리        
        COOMatrix tile_matrix = readMTXFile(file_path);
        uint32_t NEW_NNZ = tile_matrix.nnz;

        draf_result = spmv_format_transfer(
            tile_matrix.row_indices,
            tile_matrix.col_indices,
            tile_matrix.values,
            NEW_NNZ
        );
        row_used = draf_result.size();

        std::cout << "DRAF memory usage: " << row_used * 1024 << "Byte used" << std::endl;
        std::cout << "DRAF memory usage(except vec, buffer):" << (row_used * 736) << "Byte used\n" << std::endl;
        std::cout << "=======================================================" << std::endl;
    }
    
    std::cout << "All datasets processed successfully." << std::endl;
    return 0;
}
