import os
import numpy as np
from scipy.io import mmread

# 분석할 mtx 파일들
mtx_files = [
    "ASIC_100k.mtx",
    "Stanford.mtx",
    "bcsstk32.mtx",
    "cant.mtx",
    "consph.mtx",
    "crankseg_2.mtx",
    "ct20stif.mtx",
    "lhr71.mtx",
    "ohne2.mtx",
    "pdb1HYS.mtx",
    "pwtk.mtx",
    "rma10.mtx",
    "shipsec1.mtx",
    "soc-sign-epinions.mtx",
    "webbase-1M.mtx",
    "xenon2.mtx",
]

def analyze_mtx_file(file_path: str):
    """
    주어진 .mtx 파일에 대해
    1) non-zero 개수가 0~15개인 열 분류
    2) non-zero 개수가 16 이상인 열을 16으로 나눈 나머지별 분류
    결과를 반환하는 함수
    """
    # 1) .mtx 파일을 COO로 읽기
    coo = mmread(file_path).tocoo()
    
    nrows, ncols = coo.shape
    col_counts = np.zeros(ncols, dtype=int)
    
    # 열별 non-zero 개수 계산
    for c in coo.col:
        col_counts[c] += 1

    # (1) 0~15개 열 분류
    cols_by_count_under_16 = {i: [] for i in range(16)}
    for col_idx in range(ncols):
        cnt = col_counts[col_idx]
        if cnt < 16:
            cols_by_count_under_16[cnt].append(col_idx)

    # (2) 16 이상인 열만 16으로 나눈 나머지 별로 분류
    remainder_dict = {r: [] for r in range(16)}
    for col_idx in range(ncols):
        cnt = col_counts[col_idx]
        if cnt >= 16:
            r = cnt % 16
            remainder_dict[r].append(col_idx)
    
    return cols_by_count_under_16, remainder_dict, (nrows, ncols)

def main():
    for mtx_file in mtx_files:
        # 파일 존재 여부 체크
        if not os.path.exists(mtx_file):
            print(f"[WARN] 파일 {mtx_file} 이(가) 존재하지 않습니다. 건너뜁니다.")
            continue
        
        # 분석 수행
        cols_by_count_under_16, remainder_dict, shape_info = analyze_mtx_file(mtx_file)
        nrows, ncols = shape_info
        
        # 결과 출력
        print("=" * 60)
        print(f"[파일] {mtx_file}")
        print(f" - 행렬 크기: {nrows} x {ncols}")

        # (1) 0~15개 열 분류
        print("\n (1) [non-zero 개수가 0~15개인 열 분류] ")
        for k in range(16):
            count_k = len(cols_by_count_under_16[k])
            if ncols > 0:
                percent_k = (count_k / ncols) * 100.0
            else:
                percent_k = 0.0
            print(f"   - non-zero = {k}개 → 열 개수: {count_k} "
                  f"({percent_k:.2f}%)")

        # (2) 16 이상인 열을 16으로 나눈 나머지 별 분류
        print("\n (2) [16 이상인 열을 16으로 나눈 나머지 분류] ")
        for r in range(16):
            count_r = len(remainder_dict[r])
            if ncols > 0:
                percent_r = (count_r / ncols) * 100.0
            else:
                percent_r = 0.0
            print(f"   - 나머지 = {r} → 열 개수: {count_r} "
                  f"({percent_r:.2f}%)")

        print("=" * 60 + "\n")

if __name__ == "__main__":
    main()

