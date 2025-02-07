import re
import pandas as pd

def parse_output(file_path):
    """
    텍스트 파일에서 출력 내용을 파싱하여 각 데이터셋의 메모리 사용량 정보를
    리스트의 dict 형태로 반환합니다.
    """
    data = []
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 각 블록은 구분선("====...")을 기준으로 분리합니다.
    blocks = content.split("=======================================================")
    for block in blocks:
        block = block.strip()
        if not block:
            continue

        # 각 블록 내 줄 단위로 분리합니다.
        lines = block.splitlines()
        dataset_name = None
        coo_usage = None
        csr_usage = None
        csc_usage = None
        draf_usage = None
        draf_except_usage = None

        for line in lines:
            line = line.strip()
            # dataset 이름 추출
            m = re.match(r"Processing dataset:\s*(\S+)", line)
            if m:
                dataset_name = m.group(1)
                continue

            # COO 메모리 사용량 추출
            m = re.match(r"COO Format Memory Usage:\s*([\d]+)Byte used", line)
            if m:
                coo_usage = int(m.group(1))
                continue

            # CSR 메모리 사용량 추출
            m = re.match(r"CSR Format Memory Usage:\s*([\d]+)Byte used", line)
            if m:
                csr_usage = int(m.group(1))
                continue

            # CSC 메모리 사용량 추출
            m = re.match(r"CSC Format Memory Usage:\s*([\d]+)Byte used", line)
            if m:
                csc_usage = int(m.group(1))
                continue

            # DRAF 메모리 사용량 추출
            m = re.match(r"DRAF memory usage:\s*([\d]+)Byte used", line)
            if m:
                draf_usage = int(m.group(1))
                continue

            # DRAF (except vec, buffer) 메모리 사용량 추출
            m = re.match(r"DRAF memory usage\(except vec, buffer\):\s*([\d]+)Byte used", line)
            if m:
                draf_except_usage = int(m.group(1))
                continue

        # dataset 이름이 있으면 dict로 추가
        if dataset_name is not None:
            data.append({
                "Dataset": dataset_name,
                "COO Memory Usage (Bytes)": coo_usage,
                "CSR Memory Usage (Bytes)": csr_usage,
                "CSC Memory Usage (Bytes)": csc_usage,
                "DRAF Memory Usage (Bytes)": draf_usage,
                "DRAF (except vec, buffer) Memory Usage (Bytes)": draf_except_usage
            })

    return data

def main():
    # 출력 결과가 저장된 텍스트 파일 경로 (예: output.txt)
    file_path = "memory_usage.log"
    data = parse_output(file_path)

    # 데이터프레임으로 변환
    df = pd.DataFrame(data)

    # 원하는 경우 dataset 이름 순으로 정렬할 수 있습니다.
    df.sort_values("Dataset", inplace=True)

    # Excel 파일로 저장 (예: memory_usage.xlsx)
    excel_path = "memory_usage.xlsx"
    df.to_excel(excel_path, index=False)
    print(f"Excel 파일이 '{excel_path}'로 저장되었습니다.")

if __name__ == "__main__":
    main()

