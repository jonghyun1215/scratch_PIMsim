#include <iostream>
#include <vector>
#include <algorithm>
#include <set>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

#define debug 1

// Structure to hold the COO matrix data
struct COOMatrix {
    std::vector<uint32_t> row_indices;
    std::vector<uint32_t> col_indices;
    std::vector<uint16_t> values;
    uint32_t n_rows;
    uint32_t n_cols;
    uint32_t nnz;
};

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
        matrix.values.push_back(static_cast<uint16_t>(value));
    }

    file.close();
    std::cout << "Matrix loaded: " << matrix.n_rows << "x" << matrix.n_cols << " with " << matrix.nnz << " non-zero elements." << std::endl;
    return matrix;
}

// Function to count NNZ in each column
std::vector<int> countNNZPerColumnCOO(const COOMatrix& matrix) {
    std::vector<int> nnz_per_column(matrix.n_cols, 0);
    for (uint32_t col : matrix.col_indices) {
        nnz_per_column[col]++;
    }
    return nnz_per_column;
}

// Function to count row index overlap between two columns
int countRowIndexOverlapCOO(const COOMatrix& matrix, int col1, int col2) {
    std::set<uint32_t> rows_col1, rows_col2;
    for (size_t i = 0; i < matrix.nnz; ++i) {
        if (matrix.col_indices[i] == col1) rows_col1.insert(matrix.row_indices[i]);
        if (matrix.col_indices[i] == col2) rows_col2.insert(matrix.row_indices[i]);
    }
    std::vector<uint32_t> intersect;
    std::set_intersection(rows_col1.begin(), rows_col1.end(), rows_col2.begin(), rows_col2.end(),
                          std::back_inserter(intersect));
    return intersect.size();
}

// Function to select reference columns based on NNZ count
std::vector<int> selectReferenceColumnsCOO(const COOMatrix& matrix, int num_BG) {
    std::vector<std::pair<int, int>> nnz_per_column;
    auto nnz_counts = countNNZPerColumnCOO(matrix);
    for (int col = 0; col < matrix.n_cols; ++col) {
        nnz_per_column.push_back({nnz_counts[col], col});
    }
    std::sort(nnz_per_column.begin(), nnz_per_column.end(), std::greater<>());

    std::vector<int> reference_columns;
    for (int i = 0; i < num_BG; ++i) {
        reference_columns.push_back(nnz_per_column[i].second);
        std::cout << "Selected reference column: " << nnz_per_column[i].second
                  << " with " << nnz_per_column[i].first << " NNZ." << std::endl;
    }
    return reference_columns;
}

// Function to group columns based on row index similarity
std::vector<std::vector<int>> groupColumnsCOO(const COOMatrix& matrix, const std::vector<int>& reference_columns, int num_BG) {
    std::vector<std::vector<int>> grouped_columns(num_BG);
    std::vector<bool> processed_columns(matrix.n_cols, false);

    std::vector<int> division(num_BG, 2); // 조건을 1/2, 1/4, 1/8 ... 로 나누어 가며 최대한 유사한 column을 무리지어주기
    int ref_idx = 0;

    // Precompute NNZ per column
    std::vector<int> nnz_per_column = countNNZPerColumnCOO(matrix);

    // Check and mark columns with no values
    for (int col = 0; col < matrix.n_cols; ++col) {
        if (nnz_per_column[col] == 0) {
            processed_columns[col] = true;
            std::cout << "Column " << col << " has no value." << std::endl;
        }
    }

    int col = 0;
    do {
        int reference_column = reference_columns[ref_idx];
        int overlap = countRowIndexOverlapCOO(matrix, reference_column, col);
        int condition = nnz_per_column[reference_column] / division[ref_idx];

        if (overlap >= condition && !processed_columns[col]) {
            grouped_columns[ref_idx].push_back(col);
            processed_columns[col] = true;
            ref_idx = (ref_idx + 1) % num_BG;
            col = 0;
        }

        if (col < matrix.n_cols)
            ++col;

        if (col == matrix.n_cols) {
            for (int i = 0; i < matrix.n_cols; ++i) {
                if (!processed_columns[i]) {
                    col = 0;
                    if (debug)
                        std::cout << "condition: " << condition << std::endl;
                    division[ref_idx] *= 2;
                    break;
                }
            }
        }

    } while (col < matrix.n_cols);

    for (int i = 0; i < matrix.n_cols; ++i) {
        if (!processed_columns[i]) {
            std::cout << "Column " << i << " is not assigned to any bank group." << std::endl;
        }
    }

    return grouped_columns;
}


// Function to create separate COO matrices for each bank group
void createSeparateCOOMatrices(const COOMatrix& matrix,
                                const std::vector<std::vector<int>>& grouped_columns,
                                std::vector<std::vector<uint32_t>>& all_new_row_indices,
                                std::vector<std::vector<uint32_t>>& all_new_col_indices,
                                std::vector<std::vector<uint16_t>>& all_new_val) {
    all_new_row_indices.resize(grouped_columns.size());
    all_new_col_indices.resize(grouped_columns.size());
    all_new_val.resize(grouped_columns.size());

    for (size_t bg = 0; bg < grouped_columns.size(); ++bg) {
        for (int col : grouped_columns[bg]) {
            for (size_t i = 0; i < matrix.nnz; ++i) {
                if (matrix.col_indices[i] == col) {
                    all_new_val[bg].push_back(matrix.values[i]);
                    all_new_row_indices[bg].push_back(matrix.row_indices[i]);
                    all_new_col_indices[bg].push_back(col);
                }
            }
        }
    }
}

// Function to print the grouped columns
void printGroupedColumns(const std::vector<std::vector<int>>& grouped_columns) {
    for (size_t bg = 0; bg < grouped_columns.size(); ++bg) {
        std::cout << "Bank Group " << bg << ": ";
        for (int col : grouped_columns[bg]) {
            std::cout << col << " ";
        }
        std::cout << std::endl;
    }
}

// Function to print the new COO matrix
void printNewCOOMatrix(const std::vector<std::vector<uint32_t>>& all_new_row_indices,
                       const std::vector<std::vector<uint32_t>>& all_new_col_indices,
                       const std::vector<std::vector<uint16_t>>& all_new_val) {
    for (size_t bg = 0; bg < all_new_row_indices.size(); ++bg) {
        std::cout << "Bank Group " << bg << " COO Matrix:" << std::endl;

        std::cout << "Row Indices: ";
        for (const auto& row : all_new_row_indices[bg]) {
            std::cout << row << " ";
        }
        std::cout << std::endl;

        std::cout << "Col Indices: ";
        for (const auto& col : all_new_col_indices[bg]) {
            std::cout << col << " ";
        }
        std::cout << std::endl;

        std::cout << "Values: ";
        for (const auto& val : all_new_val[bg]) {
            std::cout << val << " ";
        }
        std::cout << std::endl << std::endl;
    }
}

// Main function
int main() {
    const std::string file_path = "webbase-1M.mtx"; // Update with your MTX file path
    const int num_BG = 64;

    // Step 1: Load the matrix
    COOMatrix matrix = readMTXFile(file_path);

    // Step 2: Select reference columns
    std::vector<int> reference_columns = selectReferenceColumnsCOO(matrix, num_BG);

    // Step 3: Group columns based on row index similarity
    std::vector<std::vector<int>> grouped_columns = groupColumnsCOO(matrix, reference_columns, num_BG);

    // Step 4: Print grouped columns
    printGroupedColumns(grouped_columns);

    // Step 5: Create separate COO matrices
    std::vector<std::vector<uint32_t>> all_new_row_indices;
    std::vector<std::vector<uint32_t>> all_new_col_indices;
    std::vector<std::vector<uint16_t>> all_new_val;
    createSeparateCOOMatrices(matrix, grouped_columns, all_new_row_indices, all_new_col_indices, all_new_val);

    // Step 6: Print new COO matrices
    printNewCOOMatrix(all_new_row_indices, all_new_col_indices, all_new_val);

    return 0;
}
