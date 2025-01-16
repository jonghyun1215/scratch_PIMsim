"""from scipy.io import mmread, mmwrite
import os
from scipy.sparse import coo_matrix

def tile_and_save_mtx_colwise(input_file, output_dir, num_tiles=64):
    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Load the matrix from the input .mtx file
    matrix = mmread(input_file).tocoo()  # Work in COO format for easier slicing

    # Get dimensions of the matrix
    n_rows, n_cols = matrix.shape

    # Calculate column range per tile
    cols_per_tile = n_cols // num_tiles
    remainder = n_cols % num_tiles  # Handle uneven division

    # Process each tile
    for i in range(num_tiles):
        # Calculate the column range for the current tile
        start_col = i * cols_per_tile + min(i, remainder)
        end_col = start_col + cols_per_tile + (1 if i < remainder else 0)
        
        # Extract data for the current tile
        mask = (matrix.col >= start_col) & (matrix.col < end_col)
        tile_row = matrix.row[mask]
        tile_col = matrix.col[mask]  # Keep the original column values
        tile_data = matrix.data[mask]

        # Create the tile matrix
        tile_matrix = coo_matrix((tile_data, (tile_row, tile_col)), shape=(n_rows, n_cols))

        # Save the tile to a new .mtx file
        output_file = os.path.join(output_dir, f"tile_{i+1}.mtx")
        mmwrite(output_file, tile_matrix, field='real')
        print(f"Tile {i+1} saved: {output_file}")

# Example Usage
input_mtx_file = "Stanford.mtx"  # Replace with your input .mtx file
output_directory = "output_tiles_colwise"  # Directory to save the tiles
tile_and_save_mtx_colwise(input_mtx_file, output_directory, num_tiles=64)
"""
from scipy.io import mmread, mmwrite
import os
from scipy.sparse import coo_matrix

def tile_and_save_mtx_colwise_exact(input_file, output_dir, num_tiles=64):
    # Ensure the output directory exists
    os.makedirs(output_dir, exist_ok=True)

    # Load the matrix from the input .mtx file
    matrix = mmread(input_file).tocoo()  # Work in COO format for easier slicing

    # Get dimensions of the matrix
    n_rows, n_cols = matrix.shape

    # Calculate column range per tile
    cols_per_tile = n_cols // num_tiles
    remainder = n_cols % num_tiles  # Handle uneven division

    # Process each tile
    for i in range(num_tiles):
        # Calculate the column range for the current tile
        start_col = i * cols_per_tile + min(i, remainder)
        end_col = start_col + cols_per_tile + (1 if i < remainder else 0)
       
        # Extract data for the current tile
        mask = (matrix.col >= start_col) & (matrix.col < end_col)
        tile_row = matrix.row[mask]
        tile_col = matrix.col[mask]
        tile_data = matrix.data[mask]

        # Create the tile matrix
        tile_matrix = coo_matrix((tile_data, (tile_row, tile_col)), shape=(n_rows, n_cols))

        # Save the tile to a new .mtx file
        output_file = os.path.join(output_dir, f"tile_{i}.mtx")
        mmwrite(output_file, tile_matrix, field='real')
        print(f"Tile {i+1} saved: {output_file}")

# Example Usage
input_mtx_file = "cant.mtx"  # Replace with your input .mtx file
output_directory = "output_tiles_colwise"  # Directory to save the tiles
tile_and_save_mtx_colwise_exact(input_mtx_file, output_directory, num_tiles=64)
