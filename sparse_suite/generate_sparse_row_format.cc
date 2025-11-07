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

#include <cstdint>      // uint32_t, uint16_t를 위해
#include <numeric>      // std::iota를 위해
#include <limits>       //

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
std::vector<std::vector<sparse_row_format>> loadSparseFromFile(const std::string& filename, int num_BG) {
    std::ifstream inFile(filename, std::ios::binary);

    if (!inFile.is_open()) {
        std::cerr << "Failed to open file for loading: " << filename << std::endl;
        return {};
    }

    std::vector<std::vector<sparse_row_format>> result;

    // Load the number of outer vectors
    uint32_t outerSize;
    inFile.read(reinterpret_cast<char*>(&outerSize), sizeof(uint32_t));

    result.resize(outerSize);

    for (uint32_t i = 0; i < outerSize; ++i) {
        // Load the size of each inner vector
        uint32_t innerSize;
        inFile.read(reinterpret_cast<char*>(&innerSize), sizeof(uint32_t));

        result[i].resize(innerSize);

        // Load each sparse_row_format
        for (uint32_t j = 0; j < innerSize; ++j) {
            inFile.read(reinterpret_cast<char*>(&result[i][j]), sizeof(sparse_row_format));
        }
    }

    inFile.close();
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


////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
////////////////////////FORMAT TRANSFER////////////////////////
//SW optimization 결과를 저장하기 위한 코드

void saveSPTResultToFile(const std::vector<std::vector<sparse_row_format>>& result, const std::string& filename) {
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
            outFile.write(reinterpret_cast<const char*>(&format), sizeof(sparse_row_format));
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

////////////////////////SPARSE FORMAT TRANSFER/////////////////
////////////////////////SPARSE FORMAT TRANSFER/////////////////
////////////////////////SPARSE FORMAT TRANSFER/////////////////
COOMatrix sort_coo_by_row(
    std::vector<uint32_t>& row_indices,
    std::vector<uint32_t>& col_indices,
    std::vector<uint16_t>& val,
    uint32_t& NEW_NNZ 
) {
    // 1. 0부터 NEW_NNZ-1까지의 값을 갖는 순열(permutation) 벡터를 생성합니다.
    std::vector<uint32_t> p(NEW_NNZ);
    std::iota(p.begin(), p.end(), 0); // p = {0, 1, 2, ..., NEW_NNZ-1}

    // 2. 이 순열 벡터 'p'를 정렬합니다.
    // 정렬 기준(람다 함수)은 원본 'row_indices' 벡터의 값을 참조합니다.
    std::sort(p.begin(), p.end(), [&](uint32_t i, uint32_t j) {
        // 주 정렬 기준: 행 인덱스
        if (row_indices[i] != row_indices[j]) {
            return row_indices[i] < row_indices[j];
        }
        // 보조 정렬 기준: 행이 같을 경우 열 인덱스 (안정적인 정렬)
        return col_indices[i] < col_indices[j];
    });

    // 3. 반환할 COOMatrix 객체를 준비합니다.
    COOMatrix sorted_matrix;
    sorted_matrix.nnz = NEW_NNZ;
    
    // 벡터 공간을 미리 할당합니다. (효율성)
    sorted_matrix.row_indices.resize(NEW_NNZ);
    sorted_matrix.col_indices.resize(NEW_NNZ);
    sorted_matrix.values.resize(NEW_NNZ);

    uint32_t max_row = 0;
    uint32_t max_col = 0;

    // 4. 정렬된 순열 'p'를 기반으로 새로운 벡터를 채웁니다.
    //    동시에 행과 열의 최대값을 찾아 행렬의 크기(n_rows, n_cols)를 추정합니다.
    for (uint32_t i = 0; i < NEW_NNZ; ++i) {
        uint32_t original_index = p[i]; // 정렬된 순서에 맞는 원본 인덱스

        uint32_t r = row_indices[original_index];
        uint32_t c = col_indices[original_index];
        uint16_t v = val[original_index];

        sorted_matrix.row_indices[i] = r;
        sorted_matrix.col_indices[i] = c;
        sorted_matrix.values[i] = v;

        // n_rows와 n_cols를 결정하기 위해 최대 인덱스 추적
        if (r > max_row) max_row = r;
        if (c > max_col) max_col = c;
    }

    // 5. 행렬의 크기를 설정합니다. (인덱스는 0부터 시작하므로 +1)
    //    NZE가 하나도 없는 경우 0으로 설정합니다.
    sorted_matrix.n_rows = (NEW_NNZ > 0) ? max_row + 1 : 0;
    sorted_matrix.n_cols = (NEW_NNZ > 0) ? max_col + 1 : 0;

    return sorted_matrix;
}

std::vector<sparse_row_format> spmm_format_transfer(COOMatrix& sorted_coo) 
{
    std::vector<sparse_row_format> result_vector;
    if (sorted_coo.nnz == 0) {
        return result_vector; // 비어있는 경우 빈 벡터 반환
    }
    // C++11 스타일의 {} 초기화는 모든 멤버(n_row, n_chunk, 배열)를 0으로 초기화합니다.
    sparse_row_format current_block = {};

    uint32_t nze_idx = 0; // 전체 COO 데이터를 순회하는 인덱스

    while (nze_idx < sorted_coo.nnz) {
        
        // 1. Row Grouping: 현재 행의 NZE들을 그룹화
        uint32_t current_row_idx = sorted_coo.row_indices[nze_idx];
        uint32_t row_start_nze_idx = nze_idx;
        
        // 현재 행이 끝나는 지점 탐색
        while (nze_idx < sorted_coo.nnz && sorted_coo.row_indices[nze_idx] == current_row_idx) {
            nze_idx++;
        }
        uint32_t row_end_nze_idx = nze_idx; // exclusive
        uint32_t nze_count_for_this_row = row_end_nze_idx - row_start_nze_idx;

        // 2. Segment Splitting: 이 행을 16개 NZE 단위의 세그먼트로 처리
        uint32_t nze_processed_in_row = 0;
        while (nze_processed_in_row < nze_count_for_this_row) {
            
            // 이 세그먼트가 처리할 NZE 수 (최대 16)
            uint32_t nze_for_this_segment = std::min((uint32_t)16, nze_count_for_this_row - nze_processed_in_row);

            // 3. Chunk Generation: 이 세그먼트에 필요한 리소스 계산
            //    (row_desc 1개, col_chunk 0-1개)
            uint32_t row_desc_needed = 1;
            // NZE가 9개(CHUNK_SIZE)를 초과하면 1개의 column_chunk가 필요
            uint32_t chunks_needed = (nze_for_this_segment > CHUNK_SIZE) ? 1 : 0;

            // 4. Block Splitting: 현재 블록에 이 세그먼트를 추가할 수 있는지 확인
            if (current_block.n_row + current_block.n_chunk + row_desc_needed + chunks_needed > MAX_BLOCK_PER_ROW) 
            {
                // 용량 초과. 현재 블록을 결과에 추가하고 새 블록 시작
                result_vector.push_back(current_block);
                current_block = {}; // 새 블록으로 리셋
            }

            // 5. Block Filling: 현재 블록에 세그먼트 데이터 채우기
            
            // 5-a. Row Descriptor 채우기
            uint32_t segment_start_coo_idx = row_start_nze_idx + nze_processed_in_row;
            row_descriptor rd = {}; // 0으로 초기화
            rd.row_idx = current_row_idx;
            rd.NZE_count = nze_for_this_segment;
            // rd에 저장할 NZE 수 (최대 9개)
            uint32_t nze_in_rd = std::min(nze_for_this_segment, (uint32_t)CHUNK_SIZE);

            for (uint32_t i = 0; i < nze_in_rd; ++i) {
                uint32_t coo_idx = segment_start_coo_idx + i;
                rd.NZE_val[i] = sorted_coo.values[coo_idx];
                
                // 원본 col_idx 대신, 세그먼트 내의 상대적 순서(0 ~ 8)를 저장
                rd.NZE_col_idx[i] = (uint8_t)i;
                // rd.NZE_col_idx[i] = (uint8_t)sorted_coo.col_indices[coo_idx];
            }
            
            current_block.row_desc[current_block.n_row] = rd;
            current_block.n_row++;

            // 5-b. Column Chunk 채우기 (필요한 경우)
            if (chunks_needed > 0) {
                column_chunk cc = {}; // 0으로 초기화
                // cc에 저장할 NZE 수 (최대 7개)
                uint32_t nze_in_cc = nze_for_this_segment - nze_in_rd;
                for (uint32_t i = 0; i < nze_in_cc; ++i) {
                    // rd에 저장된 NZE (9개) 이후의 인덱스부터 시작
                    cc.NZE_val[i] = sorted_coo.values[segment_start_coo_idx + nze_in_rd + i];

                    // 원본 col_idx 대신, 세그먼트 내의 상대적 순서(9 ~ 15)를 저장
                    cc.NZE_col_idx[i] = (uint8_t)(nze_in_rd + i);
                }
                
                current_block.col_chunk[current_block.n_chunk] = cc;
                current_block.n_chunk++;
                // JH debug
                // std::cout << "row per NZE over 9\n";
            }
            
            nze_processed_in_row += nze_for_this_segment;
        }
        // 다음 행 그룹으로 이동 (nze_idx는 이미 다음 행 시작점에 있음)
    }

    // 마지막으로 처리 중이던 블록 추가
    if (current_block.n_row > 0) {
        result_vector.push_back(current_block);
    }

    return result_vector;
}
////////////////////////SPARSE FORMAT TRANSFER/////////////////
////////////////////////SPARSE FORMAT TRANSFER/////////////////
////////////////////////SPARSE FORMAT TRANSFER/////////////////


////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////
void print_sparse_format(const std::vector<sparse_row_format>& custom_vec) {
    std::cout << "--- New Custom Sparse Format (Total Blocks: " 
              << custom_vec.size() << ") ---\n";

    for (size_t i = 0; i < custom_vec.size(); ++i) {
        const auto& block = custom_vec[i];
        std::cout << "\n=============================================\n";
        std::cout << "BLOCK " << i << " (n_row=" << block.n_row 
                  << ", n_chunk=" << block.n_chunk << ")\n";
        std::cout << "=============================================\n";

        uint32_t chunk_idx_tracker = 0;

        for (uint32_t j = 0; j < block.n_row; ++j) {
            const auto& rd = block.row_desc[j];
            std::cout << "  RowDesc[" << j << "]: "
                      << "row_idx=" << rd.row_idx
                      << ", NZE_count=" << (int)rd.NZE_count << "\n";
            
            uint32_t nze_in_rd = std::min((uint32_t)rd.NZE_count, (uint32_t)CHUNK_SIZE);
            std::cout << "    -> RD NZEs (" << nze_in_rd << "): ";
            for(uint32_t k=0; k < nze_in_rd; ++k) {
                // col_idx는 uint8_t이므로 (int)로 캐스팅해야 숫자로 출력됨
                std::cout << "(C=" << (int)rd.NZE_col_idx[k] << ", V=" << rd.NZE_val[k] << ") ";
            }
            std::cout << "\n";

            if (rd.NZE_count > CHUNK_SIZE) { // 10~16개 NZE -> chunk 1개 필요
                if (chunk_idx_tracker >= block.n_chunk) {
                    std::cout << "    [ERROR: Chunk index out of bounds!]\n";
                    continue;
                }
                const auto& chunk = block.col_chunk[chunk_idx_tracker];
                uint32_t nze_in_cc = rd.NZE_count - CHUNK_SIZE; // 1~7개
                
                std::cout << "    -> Chunk[" << chunk_idx_tracker << "] NZEs (" << nze_in_cc << "): ";
                for(uint32_t k=0; k < nze_in_cc; ++k) {
                    std::cout << "(C=" << (int)chunk.NZE_col_idx[k] << ", V=" << chunk.NZE_val[k] << ") ";
                }
                std::cout << "\n";
                chunk_idx_tracker++;
            }
        }
    }
    std::cout << "-------------------------------------------\n";
}
////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////
////////////////////////CODE FOR DEBUGGING/////////////////////

// Main function
int main() {
    // 처리할 데이터셋 목록
    const std::vector<std::string> dataset_names = {
        // "cora",
        // "citeseer",
        // "amazon-photo",
        "amazon-com"
        // "Pubmed",
        // "corafull",
        // "coauthor-phy",
        // "coauthor-cs",
        // "cornell",
        // "chameleon",
        // "squirrel"
    };

    const int num_tiles = 64;

    // 각 데이터셋별 처리
    for (const auto& dataset : dataset_names) {
        // 1. 파일 경로 설정
        const std::string base_path = "./suite/partitioned_default/" + dataset + "/partition_";
        const std::string output_path_b0 = "spformat_b0/tiled_sp_" + dataset + ".dat";
        const std::string output_path_b2 = "spformat_b2/tiled_sp_" + dataset + ".dat";

        // 2. 저장 벡터 초기화
        std::vector<std::vector<sparse_row_format>> spmm_format_bank0(num_tiles);
        std::vector<std::vector<sparse_row_format>> spmm_format_bank2(num_tiles);
        
        uint32_t max_size = 0;
        uint32_t max_index = 0;

        std::cout << "Processing dataset: " << dataset << std::endl;

        // 3. 타일별 처리
        for (int tile = 0; tile < num_tiles; ++tile) {
            std::string file_path = base_path + std::to_string(tile) + ".mtx";
            
            COOMatrix tile_matrix = readMTXFile(file_path);
            std::vector<COOMatrix> col_tiles = splitMatrixColumnWise(tile_matrix, 2); // 2개의 뱅크로 분할
            COOMatrix sorted_coo_b0 = sort_coo_by_row(
                col_tiles[0].row_indices,
                col_tiles[0].col_indices,
                col_tiles[0].values,
                col_tiles[0].nnz
            );
            spmm_format_bank0[tile] = spmm_format_transfer(sorted_coo_b0);

            COOMatrix sorted_coo_b2 = sort_coo_by_row(
                col_tiles[1].row_indices,
                col_tiles[1].col_indices,
                col_tiles[1].values,
                col_tiles[1].nnz
            );
            spmm_format_bank2[tile] = spmm_format_transfer(sorted_coo_b2);
        }

        // 4. 결과 저장
        saveSPTResultToFile(spmm_format_bank0, output_path_b0);
        saveSPTResultToFile(spmm_format_bank2, output_path_b2);
        std::cout << "Saved results for " << dataset << " to " << output_path_b0 << " and " << output_path_b2 << std::endl;
        
        // 5. 디버깅용 결과 비교
        // bool debug = true;
        // if (debug) {
        //     print_sparse_format(spmm_format_bank0[0]); // 첫 번째 타일의 0번 뱅크 결과만
        //     debug = false;
        // }
    }
    
    std::cout << "All datasets processed successfully." << std::endl;
    return 0;
}
