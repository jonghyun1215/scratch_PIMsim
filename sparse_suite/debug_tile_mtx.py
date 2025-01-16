from scipy.io import mmread
import os
import numpy as np

def verify_tiles(input_file, tile_dir, num_tiles=64):
    # Load the original matrix
    original_matrix = mmread(input_file).tocoo()

    # Get dimensions of the original matrix
    n_rows, n_cols = original_matrix.shape

    # Calculate column range per tile
    cols_per_tile = n_cols // num_tiles
    remainder = n_cols % num_tiles

    # Validation results
    all_tiles_match = True

    for i in range(num_tiles):
        # Calculate the column range for the current tile
        start_col = i * cols_per_tile + min(i, remainder)
        end_col = start_col + cols_per_tile + (1 if i < remainder else 0)

        # Load the current tile
        tile_file = os.path.join(tile_dir, f"tile_{i+1}.mtx")
        tile_matrix = mmread(tile_file).tocoo()

        # Filter the original matrix for the same column range
        mask = (original_matrix.col >= start_col) & (original_matrix.col < end_col)
        expected_row = original_matrix.row[mask]
        expected_col = original_matrix.col[mask]
        expected_data = original_matrix.data[mask]

        # Compare rows
        row_match = np.array_equal(tile_matrix.row, expected_row)
        col_match = np.array_equal(tile_matrix.col, expected_col)
        data_match = np.allclose(tile_matrix.data, int(expected_data))

        if not (row_match and col_match and data_match):
            print(f"Mismatch found in tile_{i+1}.mtx")
            all_tiles_match = False
            # Output details about mismatches
            if not row_match:
                print(f"  Row mismatch:")
                print(f"    Expected: {expected_row}")
                print(f"    Found: {tile_matrix.row}")
            if not col_match:
                print(f"  Column mismatch:")
                print(f"    Expected: {expected_col}")
                print(f"    Found: {tile_matrix.col}")
            if not data_match:
                print(f"  Data mismatch:")
                print(f"    Expected: {expected_data}")
                print(f"    Found: {tile_matrix.data}")
        else:
            print(f"tile_{i+1}.mtx matches the original matrix.")

    if all_tiles_match:
        print("All tiles match the original matrix.")
    else:
        print("Some tiles do not match the original matrix.")

# Example Usage
input_mtx_file = "bcsstk32.mtx"  # Replace with your original input .mtx file
#tile_directory = "output_tiles_colwise"  # Directory containing the tiles
tile_directory = "output_tiles_cpp"  # Directory containing the tiles
verify_tiles(input_mtx_file, tile_directory, num_tiles=64)

