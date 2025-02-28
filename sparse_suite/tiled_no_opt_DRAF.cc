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
        //matrix.row_indices.push_back(row - 1); // Convert to zero-based indexing
        //matrix.col_indices.push_back(col - 1); // Convert to zero-based indexing
        matrix.row_indices.push_back(row); // Convert to 1-based indexing
        matrix.col_indices.push_back(col); // Convert to 1-based indexing
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
    if(DEBUG)
        std::cout << "Data successfully loaded from " << filename << std::endl;

    return result;
}


// Function to split the matrix column-wise into `num_tiles`
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

// Function to compare a tile file with the original matrix
bool compareTileFileWithOriginal(const COOMatrix& original, const std::string& tile_file_path,
                                 uint32_t start_col, uint32_t end_col) {
    COOMatrix tile = readMTXFile(tile_file_path);

    for (size_t i = 0; i < tile.nnz; ++i) {
        uint32_t tile_row = tile.row_indices[i];
        uint32_t tile_col = tile.col_indices[i];
        uint16_t tile_value = tile.values[i];

        // Check if the tile's column is within the expected range
        if (tile_col < start_col || tile_col >= end_col) {
            std::cout << "Error: Tile column " << tile_col << " is out of range (" << start_col << ", " << end_col << ")." << std::endl;
            return false;
        }

        // Verify that the tile data matches the original matrix
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
            std::cout << "Mismatch found: (row, col, value) = (" << tile_row << ", " << tile_col << ", " << tile_value << ")" << std::endl;
            return false;
        }
    }

    return true;
}


// Function to save a COO matrix to an MTX file
void saveCOOMatrixToMTX(const std::string& file_path, const COOMatrix& matrix) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + file_path);
    }

    // Write the header
    file << "%%MatrixMarket matrix coordinate real general\n";
    file << matrix.n_rows << " " << matrix.n_cols << " " << matrix.nnz << "\n";

    // Write the COO data
    for (size_t i = 0; i < matrix.row_indices.size(); ++i) {
        file << matrix.row_indices[i] + 1 << " "  // Convert back to 1-based indexing
             << matrix.col_indices[i] + 1 << " "
             << matrix.values[i] << "\n";
    }

    file.close();
    std::cout << "Saved COO matrix to file: " << file_path << std::endl;
}

////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
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

//SW optimization 결과를 저장하기 위한 코드
void saveResultToFile(const std::vector<std::vector<re_aligned_dram_format>>& result, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::binary);

    if (!outFile.is_open()) {
        std::cerr << "Failed to open file for saving: " << filename << std::endl;
        return;
    }

    // Save the number of outer vectors
    uint32_t outerSize = result.size();
    outFile.write(reinterpret_cast<const char*>(&outerSize), sizeof(uint32_t));

    for (const auto& group : result) {
        // Save the size of each inner vector
        uint32_t innerSize = group.size();
        outFile.write(reinterpret_cast<const char*>(&innerSize), sizeof(uint32_t));

        // Save each re_aligned_dram_format
        for (const auto& format : group) {
            outFile.write(reinterpret_cast<const char*>(&format), sizeof(re_aligned_dram_format));
        }
    }

    outFile.close();
    std::cout << "Data successfully saved to " << filename << std::endl;
}
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////


////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////
bool compareReAlignedDramFormat(const re_aligned_dram_format& a, const re_aligned_dram_format& b, int groupIndex, int elementIndex) {
    // Compare col_group
    for (size_t i = 0; i < GROUP_SIZE; ++i) {
        if (a.col_group[i] != b.col_group[i]) {
            std::cout << "Difference in col_group[" << i << "] at group " << groupIndex 
                      << ", element " << elementIndex << ": " << a.col_group[i] 
                      << " != " << b.col_group[i] << std::endl;
            return false;
        }
    }

    // Compare row_buffer_empty
    if (a.row_buffer_empty != b.row_buffer_empty) {
        std::cout << "Difference in row_buffer_empty at group " << groupIndex 
                  << ", element " << elementIndex << ": " << a.row_buffer_empty 
                  << " != " << b.row_buffer_empty << std::endl;
        return false;
    }

    // Compare val
    for (size_t i = 0; i < GROUP_SIZE * PARTITION_SIZE; ++i) {
        if (a.val[i] != b.val[i]) {
            std::cout << "Difference in val[" << i << "] at group " << groupIndex 
                      << ", element " << elementIndex << ": " << a.val[i] 
                      << " != " << b.val[i] << std::endl;
            return false;
        }
    }

    // Compare row
    for (size_t i = 0; i < GROUP_SIZE * PARTITION_SIZE; ++i) {
        if (a.row[i] != b.row[i]) {
            std::cout << "Difference in row[" << i << "] at group " << groupIndex 
                      << ", element " << elementIndex << ": " << a.row[i] 
                      << " != " << b.row[i] << std::endl;
            return false;
        }
    }

    // Compare result
    for (size_t i = 0; i < GROUP_SIZE * PARTITION_SIZE; ++i) {
        if (a.result[i] != b.result[i]) {
            std::cout << "Difference in result[" << i << "] at group " << groupIndex 
                      << ", element " << elementIndex << ": " << a.result[i] 
                      << " != " << b.result[i] << std::endl;
            return false;
        }
    }

    // Compare vec
    for (size_t i = 0; i < GROUP_SIZE; ++i) {
        if (a.vec[i] != b.vec[i]) {
            std::cout << "Difference in vec[" << i << "] at group " << groupIndex 
                      << ", element " << elementIndex << ": " << a.vec[i] 
                      << " != " << b.vec[i] << std::endl;
            return false;
        }
    }

    // Compare empty
    for (size_t i = 0; i < 41; ++i) {
        if (a.empty[i] != b.empty[i]) {
            std::cout << "Difference in empty[" << i << "] at group " << groupIndex 
                      << ", element " << elementIndex << ": " << a.empty[i] 
                      << " != " << b.empty[i] << std::endl;
            return false;
        }
    }

    return true;
}

bool compareResults(
    const std::vector<std::vector<re_aligned_dram_format>>& result1,
    const std::vector<std::vector<re_aligned_dram_format>>& result2) {
    if (result1.size() != result2.size()) {
        std::cout << "Difference in outer vector size: " 
                  << result1.size() << " != " << result2.size() << std::endl;
        return false; 
    }

    for (size_t i = 0; i < result1.size(); ++i) {
        if (result1[i].size() != result2[i].size()) {
            std::cout << "Difference in inner vector size at group " << i << ": " 
                      << result1[i].size() << " != " << result2[i].size() << std::endl;
            return false;
        }

        for (size_t j = 0; j < result1[i].size(); ++j) {
            if (!compareReAlignedDramFormat(result1[i][j], result2[i][j], i, j)) {
                return false;
            }
        }
    }

    return true;
}

// Main function
int main() {
    // 처리할 데이터셋 목록
    const std::vector<std::string> dataset_names = {
        "ASIC_100k", "bcsstk32", "cant", "consph", 
        "crankseg_2", "ct20stif",  "lhr71",
        "ohne2", "pdb1HYS", "pwtk", "rma10",
        "shipsec1", "soc-sign-epinions",
        "Stanford", "webbase-1M", "xenon2"
    };

    const int num_tiles = 64;

    // 각 데이터셋별 처리
    for (const auto& dataset : dataset_names) {
        // 1. 파일 경로 설정
        const std::string base_path = "./suite/tile_wo_optimization/" + dataset + "_tiled/partition_";
        const std::string output_path = "wo_sw_opt_dat/" + dataset + ".dat";

        // 2. DRAF 저장 벡터 초기화
        std::vector<std::vector<re_aligned_dram_format>> draf_result(num_tiles);
        uint32_t max_size = 0;
        uint32_t max_index = 0;

        std::cout << "Processing dataset: " << dataset << std::endl;

        // 3. 타일별 처리
        for (int tile = 0; tile < num_tiles; ++tile) {
            std::string file_path = base_path + std::to_string(tile) + ".mtx";
            
            COOMatrix tile_matrix = readMTXFile(file_path);
            uint32_t NEW_NNZ = tile_matrix.nnz;

            draf_result[tile] = spmv_format_transfer(
                tile_matrix.row_indices,
                tile_matrix.col_indices,
                tile_matrix.values,
                NEW_NNZ
            );

            // 최대 크기 추적
            if (draf_result[tile].size() > max_size) {
                max_size = draf_result[tile].size();
                max_index = tile;
            }
        }

        // 4. 결과 저장
        saveResultToFile(draf_result, output_path);
        std::cout << "Saved results for " << dataset << " to " << output_path << std::endl;

        // 5. 디버깅용 결과 비교
        if(1){
            auto loadedResult = loadResultFromFile(output_path, 64);
            if (compareResults(draf_result, loadedResult)) {
                std::cout << "The results are same!" << std::endl;
            } else {
                std::cout << "The results are different!" << std::endl;
                exit(1);
            }
        }
    }
    
    std::cout << "All datasets processed successfully." << std::endl;
    return 0;
}
