import numpy as np
from scipy.io import mmread

# (예) "matrix.mtx" 파일을 읽어 COO 행렬로 로드
coo_matrix = mmread("webbase-1M.mtx").tocoo()

nrows, ncols = coo_matrix.shape
print("Matrix shape:", nrows, "x", ncols)

# 각 열마다 non-zero의 개수를 저장할 배열
col_counts = np.zeros(ncols, dtype=int)
for c in coo_matrix.col:
    col_counts[c] += 1

# ---------------------------------------------------
# 1. 열별 non-zero 개수가 0개, 1개, 2개, ..., 15개인 열을 각각 추출
# ---------------------------------------------------
cols_by_count_under_16 = {i: [] for i in range(16)}  # 0~15 개 분류용 딕셔너리

for col_idx in range(ncols):
    cnt = col_counts[col_idx]
    if cnt < 16:
        cols_by_count_under_16[cnt].append(col_idx)

# (결과 예시 출력)
print("\n[각 열의 non-zero 개수가 0~15개인 열 분류]")
for k in range(16):
    print(f"non-zero = {k}개인 열의 개수: {len(cols_by_count_under_16[k])}")

# ---------------------------------------------------
# 2. 열별 non-zero 개수를 16으로 나눈 나머지별로 분류 (단, non-zero 개수가 16 미만이면 제외)
# ---------------------------------------------------
remainder_dict = {r: [] for r in range(16)}

for col_idx in range(ncols):
    cnt = col_counts[col_idx]
    # 16 이상인 열만 분류
    if cnt >= 16:
        r = cnt % 16
        remainder_dict[r].append(col_idx)

print("\n[16 이상인 열들을 16으로 나눈 나머지에 따른 분류]")
for r in range(16):
    print(f"나머지 = {r}인 열의 개수: {len(remainder_dict[r])}")

