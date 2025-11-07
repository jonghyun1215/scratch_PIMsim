import os
import re

# 📁 요약 파일들이 있는 디렉토리와 최종 결과를 저장할 파일 경로를 설정합니다.
summary_dir = './summary'
output_file = './summary.txt'

desired_order = [  # 원하는 출력 순서
    'cora', 'citeseer', 'Pubmed', 'chameleon', 'squirrel', 'corafull',
    'coauthor-phy', 'coauthor-cs', 'amazon-com', 'amazon-photo'
]

# 최종 결과 리스트
processed_data = {}
# summary 디렉토리의 모든 파일을 확인합니다.
try:
    # 디렉토리 내의 .txt 파일 목록만 가져옵니다.
    summary_files = [f for f in os.listdir(summary_dir) if f.endswith('.txt')]
    if not summary_files:
        print(f"'{summary_dir}' no .txt file found.")
        exit()
except FileNotFoundError:
    print(f"Error: '{summary_dir}' directory not found.")
    exit()

# 각 txt 파일을 순회하며 정보 추출
for filename in summary_files:
    file_path = os.path.join(summary_dir, filename)
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 정규표현식을 사용하여 필요한 값을 추출합니다.
        matrix_name_match = re.search(r"Matrix:\s*(\S+)", content)
        setdata_match = re.search(r"SetData \((\d+) cycles\)", content)
        execute_match = re.search(r"Execute \((\d+) cycles\)", content)
        # getresult_match = re.search(r"GetResult \((\d+) cycles\)", content)
        
        # 모든 정보가 성공적으로 추출되었는지 확인
        # if matrix_name_match and execute_match and getresult_match:
        if matrix_name_match and setdata_match and execute_match:
            matrix_name = matrix_name_match.group(1)
            setdata_cycles = int(setdata_match.group(1))
            execute_cycles = int(execute_match.group(1))
            # getresult_cycles = int(getresult_match.group(1))
            
            # 두 사이클 값을 더합니다.
            # total_cycles = execute_cycles + getresult_cycles
            
            # 결과를 리스트에 추가
            # results.append(f"{matrix_name}, {execute_cycles}, {getresult_cycles}, {total_cycles}")
            processed_data[matrix_name] = f"{matrix_name}, {setdata_cycles}, {execute_cycles}, {setdata_cycles + execute_cycles}"
            # print(f"{matrix_name}: {total_cycles} cycles (Execute: {execute_cycles}, GetResult: {getresult_cycles})")
            print(f"{matrix_name}: {setdata_cycles + execute_cycles} cycles (SetData: {setdata_cycles}, Execute: {execute_cycles})")
        else:
            print(f"'{filename}' data not found.")

    except Exception as e:
        print(f"'{filename}' error: {e}")

final_results = []
i = 0
for name in desired_order:
    if name in processed_data:
        if name not in final_results:
            final_results.append(processed_data[name])
            i += 1

if final_results: # final_results 리스트에 데이터가 있는지 확인
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write("matrix_name, setdata_cycles, execute_cycles, total_cycles\n")
            
            # results 리스트 대신 final_results 리스트의 내용을 씀
            for line in final_results:
                f.write(line + "\n")
        print(f"\n'{output_file}' succeed.")
    except Exception as e:
        print(f"\n'{output_file}' error: {e}")
else:
    print("\nfail . (No data to write)")

# # 최종 결과를 summary.txt 파일에 저장
# if results:
#     try:
#         with open(output_file, 'w', encoding='utf-8') as f:
#             f.write("matrix_name, execute_cycles, getresult_cycles, total_cycles\n") # 헤더 추가
#             for line in results:
#                 f.write(line + "\n")
#         print(f"\n'{output_file}' succeed.")
#     except Exception as e:
#         print(f"\n'{output_file}' error: {e}")
# else:
#     print("\nfail .")

