import scipy.io
import matplotlib.pyplot as plt
import os

# 대상 폴더 경로 설정
folder_path = './'  # mtx 파일이 있는 폴더 경로

# 폴더 내 모든 파일 탐색
for file_name in os.listdir(folder_path):
    if file_name.endswith('.mtx'):  # 확장자가 .mtx인 파일만 처리
        mtx_file_path = os.path.join(folder_path, file_name)
        pdf_file_path = os.path.splitext(mtx_file_path)[0] + '.pdf'
        
        # Matrix Market 파일 로드
        matrix_data = scipy.io.mmread(mtx_file_path)
        
        # Matrix 크기 가져오기
        rows, cols = matrix_data.shape
        
        # Matrix 플롯 저장
        plt.figure(figsize=(10, 10))
        plt.spy(matrix_data, markersize=1)
        plt.title("Sparsity Pattern of the Matrix")
        plt.xlabel(f"Columns (size: {cols})")
        plt.ylabel(f"Rows (size: {rows})")
        plt.savefig(pdf_file_path)
        plt.close()  # 플롯 닫기
        
        print(f"Saved: {pdf_file_path}")

print("All .mtx files have been processed and saved as .pdf.")
