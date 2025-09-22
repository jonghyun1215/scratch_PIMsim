#!/bin/bash
# extract_cycles.sh

#output_folder="SparsePIM_log"
output_folder=$PWD
result_file="total_result.log"

# 이전 결과 파일이 있다면 삭제
rm -f "$result_file"

# 모든 wo_*.log 파일에 대해 반복
for logfile in "$output_folder"/wo_*.log; do
    # 파일명에서 matrix base 이름 추출 (예: wo_ASIC_100k.log -> ASIC_100k)
    matrix=$(basename "$logfile" | sed 's/wo_\(.*\)\.log/\1/')
    
    echo "Matrix: $matrix" >> "$result_file"
    # "cycle"이 포함된 줄 추출하여 추가
    grep "cycle" "$logfile" >> "$result_file"
    
    # "Total accumulation count:" 라인이 있다면 해당 숫자 추출 (예: Total accumulation count: 37612)
    accumulation=$(grep "Total accumulation count:" "$logfile" | awk -F': ' '{print $2}')
    echo "Total accumulation count: $accumulation" >> "$result_file"
    
    echo "------------------------------" >> "$result_file"
done

echo "Cycle results aggregated into $result_file"

