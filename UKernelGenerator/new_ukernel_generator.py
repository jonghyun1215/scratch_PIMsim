import re

# Operation Code (op_code) 계산 함수
def op_code(word):
    if "NOP" in word: 
        return 0b0000 << 28  # 0
    elif "JUMP" in word:
        return 0b0001 << 28  # 1
    elif "EXIT" in word:
        return 0b0010 << 28  # 2
    elif "MOV" in word:
        return 0b0100 << 28  # 4
    elif "FILL" in word:
        return 0b0101 << 28  # 5
    elif "ADD" in word:
        return 0b1000 << 28  # 8
    elif "MUL" in word:
        return 0b1001 << 28  # 9
    elif "MAC" in word:
        return 0b1010 << 28  # 10
    elif "MAD" in word:
        return 0b1011 << 28  # 11
    elif "SACC" in word:  # 추가된 SACC 연산 처리
        return 0b1100 << 28  # 12
    else:
        raise ValueError(f"Unknown operation: {word}")

# AAM 플래그 계산 함수
def aam_code(word):
    if "AAM" in word or "(A" in word:
        return 1 << 15
    else:
        return 0

# Source Code (src_code) 계산 함수
def src_code(word, loc):
    shifter = 25 - loc * 3  # 소스 위치 비트 시프트
    idx_shifter = 8 - loc * 4  # 인덱스 위치 비트 시프트
    out = 0b0

    # 레지스터 및 BANK 처리
    if "BANK" in word:
        out = 0b000 << shifter
    elif "GRF_A" in word:
        out = 0b001 << shifter
    elif "GRF_B" in word:
        out = 0b010 << shifter
    elif "SRF_A" in word:
        out = 0b011 << shifter
    elif "SRF_M" in word:
        out = 0b100 << shifter
    # TW added
    # To support move data from BANK to L_IQ & R_IQ
    elif "L_IQ" in word:  # 추가된 L_IQ
        out = 0b101 << shifter
    elif "R_IQ" in word:  # 추가된 R_IQ
        out = 0b110 << shifter
    else:
        idx_shifter = 11 - loc * 11  # 인덱스 비트 계산 조정

    # 인덱스 처리
    if any(value.isdigit() for value in word):
        tmp = int(re.sub(r'[^0-9]', '', word))  # 숫자 추출
        if "-" in word:  # 음수 처리
            tmp = tmp | (0b1 << 7)
        if "[A" not in word:  # [A]가 없는 경우 인덱스 비트 설정
            tmp = tmp | (0b1 << 3)
        tmp = tmp << idx_shifter
        out = out | tmp

    return out

# uKernel 변환 함수
def convert_to_ukernel(words):
    ukernel_code = 1 << 32  # 상위 비트 초기화
    ukernel_code = ukernel_code | op_code(words[0])
    ukernel_code = ukernel_code | aam_code(words[0])
    if len(words) >= 2:
        ukernel_code = ukernel_code | src_code(words[1], 0)
    if len(words) >= 3:
        ukernel_code = ukernel_code | src_code(words[2], 1)
    if len(words) >= 4:
        ukernel_code = ukernel_code | src_code(words[3], 2)
    ukernel_code = ukernel_code - (1 << 32)  # 상위 비트 제거
    ukernel_code = str(bin(ukernel_code))
    ukernel_code = '0b' + '0' * (34 - len(ukernel_code)) + ukernel_code[2:]  # 34비트 패딩
    return ukernel_code

# 파일 처리 및 결과 생성
def process_file(input_filename, output_filename):
    with open(input_filename, "r") as input_file, open(output_filename, "w") as output_file:
        idx = 0
        for line in input_file:
            words = line.split()
            try:
                ukernel_code = convert_to_ukernel(words)
                output_file.write(f"ukernel[{idx}]={ukernel_code};  // {line}")
                idx += 1
            except ValueError as e:
                output_file.write(f"Error processing line {idx}: {e}\n")
                idx += 1

# 실행
if __name__ == "__main__":
    input_filename = "input.txt"
    output_filename = "output.txt"
    process_file(input_filename, output_filename)
