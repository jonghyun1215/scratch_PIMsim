#!/bin/bash
# run_all.sh

# 결과 파일들을 저장할 폴더 생성
opt_output_folder="SparsePIM_opt_log"
no_opt_output_folder="SparsePIM_no_opt_log"
mkdir -p "$opt_output_folder"
mkdir -p "$no_opt_output_folder"

# 처리할 matrix base 이름 배열 (확장자 제거된 이름)
matrix_bases=("ASIC_100k" "bcsstk32" "cant" "consph" "crankseg_2" "ct20stif" "lhr71" "ohne2" "pdb1HYS" "pwtk" "rma10" "shipsec1" "soc-sign-epinions" "Stanford" "webbase-1M" "xenon2")

# config 파일 및 PIM API (필요시 수정)
config_file="../configs/HBM2_4Gb_test.ini"
pim_api="spmv"

for matrix in "${matrix_bases[@]}"; do
    echo "========================================"
    echo "Running simulation for matrix: $matrix with SW_OPT enabled"
    
    # SW_OPT enabled
    ./pimdramsim3main "$config_file" --pim-api="$pim_api" -m "$matrix" -w > run.log
    mv run.log "${opt_output_folder}/wo_${matrix}.log"
    echo "Created log file: ${opt_output_folder}/wo_${matrix}.log"
    
    if [ -f dramsim3.txt ]; then
        mv dramsim3.txt "${opt_output_folder}/${matrix}_dramsim3.txt"
        echo "Renamed dramsim3.txt to ${opt_output_folder}/${matrix}_dramsim3.txt"
    else
        echo "dramsim3.txt not found!"
    fi
    
    echo "Running simulation for matrix: $matrix with SW_OPT disabled"
    
    # SW_OPT disabled
    ./pimdramsim3main "$config_file" --pim-api="$pim_api" -m "$matrix" > run.log
    mv run.log "${no_opt_output_folder}/wo_${matrix}.log"
    echo "Created log file: ${no_opt_output_folder}/wo_${matrix}.log"
    
    if [ -f dramsim3.txt ]; then
        mv dramsim3.txt "${no_opt_output_folder}/${matrix}_dramsim3.txt"
        echo "Renamed dramsim3.txt to ${no_opt_output_folder}/${matrix}_dramsim3.txt"
    else
        echo "dramsim3.txt not found!"
    fi
    
    echo "Simulation for $matrix completed."
    echo "========================================"
done

echo "All simulations completed. Logs are saved in $opt_output_folder and $no_opt_output_folder."

