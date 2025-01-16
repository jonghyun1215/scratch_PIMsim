#include <cstdint>
#include <iostream>
#include <vector>
#include <algorithm>
#include <set>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <map>

// Structure to hold the COO matrix data
struct COOMatrix {
    std::vector<uint32_t> row_indices;
    std::vector<uint32_t> col_indices;
    std::vector<float> values;
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
        //matrix.values.push_back(static_cast<uint16_t>(value));
        matrix.values.push_back(value);
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

// Function to sort COOMatrix by column NNZ in descending order
COOMatrix sortCOOMatrixByColumnNNZ(const COOMatrix& matrix) {
    // Count NNZ per column
    auto nnz_per_col = countNNZPerColumnCOO(matrix);

    // Sort columns by NNZ count in descending order
    std::vector<uint32_t> sorted_cols(matrix.n_cols);
    for (uint32_t i = 0; i < matrix.n_cols; ++i) {
        sorted_cols[i] = i;
    }
    std::sort(sorted_cols.begin(), sorted_cols.end(),
              [&nnz_per_col](uint32_t col1, uint32_t col2) {
                  return nnz_per_col[col1] > nnz_per_col[col2];
              });

    // Map old columns to new order
    std::map<uint32_t, uint32_t> col_map;
    for (uint32_t new_col = 0; new_col < sorted_cols.size(); ++new_col) {
        col_map[sorted_cols[new_col]] = new_col;
    }

    // Rebuild COOMatrix with sorted columns
    COOMatrix sorted_matrix;
    sorted_matrix.n_rows = matrix.n_rows;
    sorted_matrix.n_cols = matrix.n_cols;
    sorted_matrix.nnz = matrix.nnz;

    std::vector<std::tuple<uint32_t, uint32_t, float>> entries(matrix.nnz);
    for (uint32_t i = 0; i < matrix.nnz; ++i) {
        entries[i] = {matrix.row_indices[i], matrix.col_indices[i], matrix.values[i]};
    }

    std::sort(entries.begin(), entries.end(),
              [&col_map](const auto& a, const auto& b) {
                  uint32_t col_a = col_map[std::get<1>(a)];
                  uint32_t col_b = col_map[std::get<1>(b)];
                  return col_a < col_b;
              });

    for (const auto& [row, col, value] : entries) {
        sorted_matrix.row_indices.push_back(row);
        sorted_matrix.col_indices.push_back(col);
        sorted_matrix.values.push_back(value);
    }

    return sorted_matrix;
}

// Function to save a COO matrix to an MTX file
void saveCOOMatrixToMTX(const std::string& file_path,
                        const COOMatrix& matrix) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file for writing: " + file_path);
    }

    // Write the header
    file << "%%MatrixMarket matrix coordinate real general\n";
    file << matrix.n_rows << " " << matrix.n_cols << " " << matrix.nnz << "\n";

    // Write the COO data
    for (size_t i = 0; i < matrix.nnz; ++i) {
        file << matrix.row_indices[i] + 1 << " "  // Convert back to 1-based indexing
             << matrix.col_indices[i] + 1 << " "
             << matrix.values[i] << "\n";
    }

    file.close();
    std::cout << "Saved COO matrix to file: " << file_path << std::endl;
}

// Main function
int main() {
    const std::string file_path = "./consph.mtx"; // Update with your MTX file path

    // Step 1: Load the matrix
    COOMatrix matrix = readMTXFile(file_path);

    // Step 2: Sort matrix by column NNZ
    COOMatrix sorted_matrix = sortCOOMatrixByColumnNNZ(matrix);
    std::cout << "Sorting finished" << std::endl;

    // Step 3: Save sorted COO matrix to MTX file
    const std::string sorted_file_path = "./sorted_consph.mtx";
    saveCOOMatrixToMTX(sorted_file_path, sorted_matrix);

    return 0;
}