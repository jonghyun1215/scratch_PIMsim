import os
import re

# 로그 파일이 있는 디렉토리와 요약 파일을 저장할 디렉토리를 설정합니다.
log_dir = '/home/jonghyun/sparsePIM/scratch_PIMsim/build/log'
summary_dir = '/home/jonghyun/sparsePIM/scratch_PIMsim/build/summary'

# 요약 파일을 저장할 디렉토리가 없으면 생성합니다.
os.makedirs(summary_dir, exist_ok=True)

# 로그 디렉토리의 모든 파일을 확인합니다.
try:
    log_files = [f for f in os.listdir(log_dir) if f.endswith('.log')]
    if not log_files:
        print(f"'{log_dir}' No .log files found.")
        exit()
except FileNotFoundError:
    print(f"Error: log directory '{log_dir}' not found. Please check the path.")
    exit()

# 각 로그 파일을 순회하며 요약 작업을 수행합니다.
for filename in log_files:
    log_file_path = os.path.join(log_dir, filename)
    
    try:
        with open(log_file_path, 'r', encoding='utf-8') as f:
            content = f.read()

        # 정규표현식을 사용하여 원하는 요약 블록을 찾습니다.
        # 'Matrix:'로 시작해서 'Total accumulation count: 숫자'로 끝나는 부분을 찾습니다.
        pattern = re.compile(r"(Matrix:.*?Total accumulation count: \d+)", re.DOTALL)
        match = pattern.search(content)

        if match:
            summary_text = match.group(1)
            
            # 원본 파일 이름에서 확장자(.log)를 제거합니다. (예: 'cant.log' -> 'cant')
            base_name = os.path.splitext(filename)[0]
            
            # 요약 파일 이름을 생성합니다. (예: 'summary_cant.txt')
            summary_filename = f"summary_{base_name}.txt"
            summary_file_path = os.path.join(summary_dir, summary_filename)
            
            # 요약 파일에 추출한 내용을 씁니다.
            with open(summary_file_path, 'w', encoding='utf-8') as summary_file:
                summary_file.write(summary_text)
                
            print(f"'{summary_file_path}' completed!")
        else:
            print(f"'{filename}' No summary information found.")

    except Exception as e:
        print(f"Error processing '{filename}': {e}")

print("\nAll tasks completed.")