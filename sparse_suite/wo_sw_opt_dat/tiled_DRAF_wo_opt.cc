#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>
#include "../sw_full_stack.h"

#define DEBUG 0
#define STORE 0

// (헤더 파일에 정의된) COOMatrix, COOMatrixInfo, re_aligned_dram_format 등은 sw_full_stack.h에 포함되어 있다고 가정합니다.

// customRound 함수
uint16_t customRound(float value) {
    if (value > 0) {
        return static_cast<uint16_t>(std::ceil(value));
    } else {
        return static_cast<uint16_t>(std::floor(value));
    }
}

// Matrix Market (MTX) 파일을 읽어 COOMatrix를 구성하는 함수
COOMatrix readMTXFile(const std::string& file_path) {
    COOMatrix matrix;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + file_path);
    }

    std::string line;
    bool is_comment = true;

    // 헤더 및 메타데이터 읽기
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

    // COO 데이터 읽기
    uint32_t row, col;
    float value;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> row >> col >> value;
        // 내부 처리는 1-based indexing 유지
        matrix.row_indices.push_back(row);
        matrix.col_indices.push_back(col);
        matrix.values.push_back(customRound(value));
    }

    file.close();
    std::cout << "Matrix loaded: " << matrix.n_rows << "x" << matrix.n_cols 
              << " with " << matrix.nnz << " non-zero elements." << std::endl;
    return matrix;
}

// MTX 파일 정보만 빠르게 읽어오는 함수
COOMatrixInfo readMTXFileInformation(const std::string& file_path) {
    COOMatrixInfo matrix;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + file_path);
    }

    std::string line;
    bool is_comment = true;

    while (is_comment && std::getline(file, line)) {
        if (line[0] != '%') {
            is_comment = false;
            std::istringstream iss(line);
            iss >> matrix.n_rows >> matrix.n_cols >> matrix.nnz;
        }
    }
    file.close();
    std::cout << "Matrix information loaded: " << matrix.n_rows << "x" 
              << matrix.n_cols << " with " << matrix.nnz << " non-zero elements." << std::endl;
    return matrix;
}

// dat 확장자 파일로 저장된 sw optimization 결과를 불러오기 위한 함수
std::vector<std::vector<re_aligned_dram_format>> loadResultFromFile(const std::string& filename, int num_BG) {
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "Failed to open file for loading: " << filename << std::endl;
        return {};
    }

    std::vector<std::vector<re_aligned_dram_format>> result;
    uint32_t outerSize;
    inFile.read(reinterpret_cast<char*>(&outerSize), sizeof(uint32_t));
    result.resize(outerSize);

    for (uint32_t i = 0; i < outerSize; ++i) {
        uint32_t innerSize;
        inFile.read(reinterpret_cast<char*>(&innerSize), sizeof(uint32_t));
        result[i].resize(innerSize);
        for (uint32_t j = 0; j < innerSize; ++j) {
            inFile.read(reinterpret_cast<char*>(&result[i][j]), sizeof(re_aligned_dram_format));
        }
    }
    inFile.close();
    std::cout << "Data successfully loaded from " << filename << std::endl;
    return result;
}

// 행렬을 열 기준으로 num_tiles 개의 타일로 분할하는 함수
std::vector<COOMatrix> splitMatrixColumnWise(const COOMatrix& matrix, int num_tiles) {
    uint32_t cols_per_tile = matrix.n_cols / num_tiles;
    uint32_t remainder = matrix.n_cols % num_tiles;
    std::vector<COOMatrix> tiles(num_tiles);

    for (int tile = 0; tile < num_tiles; ++tile) {
        uint32_t start_col = tile * cols_per_tile + std::min(tile, (int)remainder);
        uint32_t end_col = start_col + cols_per_tile + (tile < remainder ? 1 : 0);
        COOMatrix tile_matrix;
        tile_matrix.n_rows = matrix.n_rows;
        tile_matrix.n_cols = matrix.n_cols;

        for (size_t i = 0; i < matrix.nnz; ++i) {
            if (matrix.col_indices[i] >= start_col && matrix.col_indices[i] < end_col) {
                tile_matrix.row_indices.push_back(matrix.row_indices[i]);
                tile_matrix.col_indices.push_back(matrix.col_indices[i]);
                tile_matrix.values.push_back(matrix.values[i]);
            }
        }
        tile_matrix.nnz = tile_matrix.row_indices.size();
        tiles[tile] = tile_matrix;
    }
    return tiles;
}

// (필요시) 타일 파일과 원본 행렬을 비교하는 함수
bool compareTileFileWithOriginal(const COOMatrix& original, const std::string& tile_file_path,
                                 uint32_t start_col, uint32_t end_col) {
    COOMatrix tile = readMTXFile(tile_file_path);
    for (size_t i = 0; i < tile.nnz; ++i) {
        uint32_t tile_row = tile.row_indices[i];
        uint32_t tile_col = tile.col_indices[i];
        uint16_t tile_value = tile.values[i];
        if (tile_col < start_col || tile_col >= end_col) {
            std::cout << "Error: Tile column " << tile_col << " is out of range (" 
                      << start_col << ", " << end_col << ")." << std::endl;
            return false;
        }
        bool found = false;
        for (size_t j = 0; j < original.nnz; ++j) {
            if (original.row_indices[j] == tile_row &&
                original.col_indices[j] == tile_col &&
                original.values[j] == tile_value) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "Mismatch found: (row, col, value) = (" 
                      << tile_row << ", " << tile_col << ", " << tile_value << ")" << std::endl;
            return false;
        }
    }
    return true;
}

// COO 행렬을 MTX 파일로 저장하는 함수
void saveCOOMatrixToMTX(const std::string& file_path, const COOMatrix& matrix) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + file_path);
    }
    file << "%%MatrixMarket matrix coordinate real general\n";
    file << matrix.n_rows << " " << matrix.n_cols << " " << matrix.nnz << "\n";
    for (size_t i = 0; i < matrix.row_indices.size(); ++i) {
        file << matrix.row_indices[i] + 1 << " "  // 1-based indexing으로 변환
             << matrix.col_indices[i] + 1 << " "
             << matrix.values[i] << "\n";
    }
    file.close();
    std::cout << "Saved COO matrix to file: " << file_path << std::endl;
}

//////////////////////// FORMAT TRANSFER ////////////////////////
typedef struct align_row_buffer {
    uint16_t val[PARTITION_SIZE]; // 실제 타입은 fp16 (여기서는 uint16_t 사용)
    uint32_t row[PARTITION_SIZE];
} align_row_buffer;

typedef struct align_dram_row {
    uint32_t col_group[GROUP_SIZE];
    uint32_t row_buffer_empty;
    align_row_buffer row_buffer[GROUP_SIZE];
    uint16_t result[GROUP_SIZE * PARTITION_SIZE];
} align_dram_row;

// re_aligned_dram_format으로 변환하는 함수
std::vector<re_aligned_dram_format> spmv_format_transfer(
    std::vector<uint32_t>& row_indices, 
    std::vector<uint32_t>& col_indices, 
    std::vector<uint16_t>& val, 
    uint32_t& NEW_NNZ) 
{
    std::vector<align_row_buffer> row_buffer(NEW_NNZ);
    std::vector<uint32_t> col_index(NEW_NNZ, 0);
    uint32_t t = 0;
    uint32_t nnz_count = 0;

    // row_buffer 초기화
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

    std::vector<align_dram_row> dram_row((t / GROUP_SIZE) + 1);
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

    for(size_t i = 0; i <= t; i++) {
        int row_index = i / GROUP_SIZE;
        int buffer_index = i % GROUP_SIZE;
        dram_row[row_index].row_buffer[buffer_index] = row_buffer[i];
        dram_row[row_index].col_group[buffer_index] = col_index[i];
    }
    if(DEBUG) std::cout << "dram row filled" << std::endl;

    std::vector<re_aligned_dram_format> store_format((t / GROUP_SIZE) + 1);
    for(uint32_t i = 0; i <= t / GROUP_SIZE; i++) {
        for(uint32_t j = 0; j < GROUP_SIZE; j++) {
            for(uint32_t k = 0; k < PARTITION_SIZE; k++) {
                uint32_t buffer_index = j * PARTITION_SIZE + k;
                store_format[i].row[buffer_index] = dram_row[i].row_buffer[j].row[k];
                // 모든 value 값을 1로 설정 (TEST용)
                if(store_format[i].row[buffer_index] != 0)
                    store_format[i].val[buffer_index] = 1;
                else
                    store_format[i].val[buffer_index] = 0;
            }
            store_format[i].col_group[j] = dram_row[i].col_group[j];
            if(store_format[i].col_group[j] != 0) {
                store_format[i].vec[j] = 1;
            }
            else {
                store_format[i].vec[j] = 0;
            }
        }
    }
    if(DEBUG) std::cout << "store format filled" << std::endl;

    return store_format;
}

// 결과를 .dat 파일로 저장하는 함수
void saveResultToFile(const std::vector<std::vector<re_aligned_dram_format>>& result, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return;
    }
    uint32_t outerSize = result.size();
    outFile.write(reinterpret_cast<const char*>(&outerSize), sizeof(uint32_t));
    for (const auto& group : result) {
        uint32_t innerSize = group.size();
        outFile.write(reinterpret_cast<const char*>(&innerSize), sizeof(uint32_t));
        for (const auto& format : group) {
            outFile.write(reinterpret_cast<const char*>(&format), sizeof(re_aligned_dram_format));
        }
    }
    outFile.close();
    std::cout << "Data successfully saved to " << filename << std::endl;
}

//////////////////////// CODE FOR DEBUGGING //////////////////////
// (비교 및 디버깅 함수들 생략...)
// ...

//////////////////////// MAIN //////////////////////
int main() {
    // 입력 및 출력 경로 정의
    const std::string input_dir = "/home/taewoon/second_drive/scratch_PIMsim/sparse_suite/suite/sorted_suite/";
    const std::string output_dir = "/home/taewoon/second_drive/scratch_PIMsim/sparse_suite/wo_sw_opt_dat/";
    const int num_tiles = 64;

    // 처리할 파일 목록
    std::vector<std::string> filenames = {
        "ASIC_100k_new.mtx",
        "bcsstk32_new.mtx",
        "cant_new.mtx",
        "consph_new.mtx",
        "crankseg_2_new.mtx",
        "ct20stif_new.mtx",
        "lhr71_new.mtx",
        "ohne2_new.mtx",
        "pdb1HYS_new.mtx",
        "pwtk_new.mtx",
        "rma10_new.mtx",
        "shipsec1_new.mtx",
        "soc-sign-epinions_new.mtx",
        "sorted_consph_new.mtx",
        "Stanford_new.mtx",
        "webbase-1M_new.mtx",
        "xenon2_new.mtx"
    };

    // 각 파일에 대해 순차 처리
    for (const auto &filename : filenames) {
        std::string file_path = input_dir + filename;
        std::cout << "\nProcessing file: " << file_path << std::endl;

        // Step 1: 원본 행렬 로드
        COOMatrix original_matrix = readMTXFile(file_path);

        // Step 2: 행렬을 num_tiles 개의 타일로 분할
        std::vector<COOMatrix> tiles = splitMatrixColumnWise(original_matrix, num_tiles);

        // Step 3: 각 타일을 DRAM 정렬 포맷으로 변환
        std::vector<std::vector<re_aligned_dram_format>> draf_result(num_tiles);
        for (int tile = 0; tile < num_tiles; ++tile) {
            std::cout << "Tile " << tile << " processing..." << std::endl;
            uint32_t NEW_NNZ = tiles[tile].nnz;
            draf_result[tile] = spmv_format_transfer(tiles[tile].row_indices, tiles[tile].col_indices, tiles[tile].values, NEW_NNZ);
        }

        // (원하는 경우 STORE 옵션에 따라 타일을 .mtx 파일로 저장 및 비교 가능)

        // Step 4: 출력 파일명 생성
        // 파일명에서 "_new" 부분을 제거하고 확장자를 .dat로 변경합니다.
        std::string out_filename = filename;
        size_t pos = out_filename.find("_new");
        if (pos != std::string::npos) {
            out_filename.erase(pos, 4); // "_new" 제거
        }
        pos = out_filename.rfind('.');
        if (pos != std::string::npos) {
            out_filename.replace(pos, out_filename.length() - pos, ".dat");
        } else {
            out_filename += ".dat";
        }
        std::string output_file_path = output_dir + out_filename;

        // Step 5: 변환 결과를 .dat 파일로 저장
        saveResultToFile(draf_result, output_file_path);

        std::cout << "Finished processing file: " << file_path << std::endl;
    }

    std::cout << "\nAll files processed." << std::endl;
    return 0;
}

