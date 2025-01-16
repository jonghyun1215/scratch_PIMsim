import scipy.io
import matplotlib.pyplot as plt
import numpy as np

# Load the Matrix Market file
matrix_file_path = 'cant.mtx'
matrix_data = scipy.io.mmread(matrix_file_path)

rows, cols = matrix_data.shape

# Count non-zero values per column
def count_nnz_per_column(matrix):
    """
    Count the number of non-zero values in each column of the matrix.
    """
    # Ensure the matrix is in CSC format
    matrix_csc = matrix.tocsc()

    # Get the NNZ count for each column
    nnz_per_column = matrix_csc.getnnz(axis=0)
    return nnz_per_column

# Get non-zero counts per column
nnz_per_column = count_nnz_per_column(matrix_data)

# Print non-zero values per column
print("Number of non-zero values per column:")
for col_idx, nnz_count in enumerate(nnz_per_column):
    print(f"Column {col_idx + 1}: {nnz_count} non-zero values")

# Optional: Print total number of non-zero values
total_nnz = nnz_per_column.sum()
print(f"\nTotal number of non-zero values (NNZ): {total_nnz}")

# Plot the matrix as a spy plot (sparsity pattern)
plt.figure(figsize=(10, 10))
plt.spy(matrix_data, markersize=0.01)
plt.title("Sparsity Pattern of the Matrix: " + matrix_file_path)
plt.xlabel(f"Columns (size: {cols})")
plt.ylabel(f"Rows (size: {rows})")
plt.show()

