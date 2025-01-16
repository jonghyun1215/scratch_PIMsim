import scipy.io
import matplotlib.pyplot as plt
import numpy as np

# Load the Matrix Market file
matrix_file_path = 'cant.mtx'
matrix_data = scipy.io.mmread(matrix_file_path)

rows, cols = matrix_data.shape

# Count non-zero values per row
def count_nnz_per_row(matrix):
    """
    Count the number of non-zero values in each row of the matrix.
    """
    # Ensure the matrix is in CSR format
    matrix_csr = matrix.tocsr()

    # Get the NNZ count for each row
    nnz_per_row = matrix_csr.getnnz(axis=1)
    return nnz_per_row

# Get non-zero counts per row
nnz_per_row = count_nnz_per_row(matrix_data)

# Print non-zero values per row
print("Number of non-zero values per row:")
for row_idx, nnz_count in enumerate(nnz_per_row):
    print(f"Row {row_idx + 1}: {nnz_count} non-zero values")

# Optional: Print total number of non-zero values
total_nnz = nnz_per_row.sum()
print(f"\nTotal number of non-zero values (NNZ): {total_nnz}")

# Plot the matrix as a spy plot (sparsity pattern)
plt.figure(figsize=(10, 10))
plt.spy(matrix_data, markersize=0.01)
plt.title("Sparsity Pattern of the Matrix: " + matrix_file_path)
plt.xlabel(f"Columns (size: {cols})")
plt.ylabel(f"Rows (size: {rows})")
plt.show()

