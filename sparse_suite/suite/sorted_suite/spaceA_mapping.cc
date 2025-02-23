#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>

// 하나의 COO 원소
// row, col: 4바이트(int), val: 2바이트(short)
struct CooElement {
    int row;
    int col;
    short val;
};

// 한 파일을 처리하여 사용된 1KB Chunk 수를 반환하는 함수
long long processFile(const std::string &filename) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cerr << "[Error] Failed to open file: " << filename << "\n";
        return 0; // 열 수 없으면 0
    }

    // MTX 헤더/주석 스킵
    std::string line;
    while (true) {
        std::streampos prevPos = fin.tellg();
        if (!std::getline(fin, line)) {
            std::cerr << "[Error] Unexpected end of file in " << filename << "\n";
            return 0;
        }
        if (!line.empty() && line[0] != '%') {
            fin.seekg(prevPos); 
            break;
        }
    }

    // 첫 번째 숫자 라인에서 행, 열, 비제로 개수를 읽음
    int numRows, numCols, numNonZeros;
    fin >> numRows >> numCols >> numNonZeros;

    // COO 데이터를 저장할 벡터
    std::vector<CooElement> cooData;
    cooData.reserve(numNonZeros);

    // (row, col, val) 읽어들임
    for (int i = 0; i < numNonZeros; i++) {
        int r, c;
        double v; // MTX에 double/float 형태가 있을 수 있으므로 우선 double로 받음
        fin >> r >> c >> v;
        // 필요 시 1-based -> 0-based 변환
        // r--; c--;

        // val 을 2바이트(short)로 캐스팅 (단순 예시)
        short valShort = static_cast<short>(v);
        cooData.push_back({r, c, valShort});
    }
    fin.close();

    // col 기준 정렬 (col이 같으면 row 기준으로)
    std::sort(cooData.begin(), cooData.end(),
              [](const CooElement &a, const CooElement &b) {
                  if (a.col == b.col) return a.row < b.row;
                  return a.col < b.col;
              });

    const int CHUNK_SIZE = 170;
    long long totalChunkCount = 0;

    // flushChunk 함수
    // 내부 데이터를 실제로 출력하지 않고,
    // 170개씩 분할하여 Chunk 수만 셉니다.
    auto flushChunk = [&](int /*currentCol*/, 
                          const std::vector<int> &rowsBuffer,
                          const std::vector<short> &valsBuffer)
    {
        size_t totalSize = rowsBuffer.size();
        size_t startIdx = 0;
        while (startIdx < totalSize) {
            size_t thisChunkCount = std::min<size_t>(CHUNK_SIZE, totalSize - startIdx);
            // "이 Chunk를 하나 사용"했다고 가정
            totalChunkCount++;
            startIdx += thisChunkCount;
        }
    };

    // col 별로 Buffers를 쌓았다가 flushChunk로 170개씩 나눔
    int currentCol = -1;
    std::vector<int> rowsBuffer;
    std::vector<short> valsBuffer;

    for (auto &elem : cooData) {
        if (elem.col != currentCol) {
            // 컬럼이 바뀌면 기존 컬럼 flush
            if (currentCol != -1) {
                flushChunk(currentCol, rowsBuffer, valsBuffer);
            }
            currentCol = elem.col;
            rowsBuffer.clear();
            valsBuffer.clear();
        }
        rowsBuffer.push_back(elem.row);
        valsBuffer.push_back(elem.val);
    }
    // 마지막 컬럼 flush
    if (currentCol != -1) {
        flushChunk(currentCol, rowsBuffer, valsBuffer);
    }

    return totalChunkCount;
}

int main() {
    // 처리할 MTX 파일 목록
	std::vector<std::string> fileList = {
		"cant_new.mtx",
		"crankseg_2_new.mtx",
		"lhr71_new.mtx",
		"pdb1HYS_new.mtx",
		"rma10_new.mtx",
		"soc-sign-epinions_new.mtx",
		"Stanford_new.mtx",
		"bcsstk32_new.mtx",
		"consph_new.mtx",
		"ct20stif_new.mtx",
		"ohne2_new.mtx",
		"pwtk_new.mtx",
		"shipsec1_new.mtx",
		"ASIC_100k_new.mtx",
		"xenon2_new.mtx",
		"webbase-1M_new.mtx"
	};

    // 각 파일에 대해 순차 처리
    for (const auto &filename : fileList) {
        std::cout << "=====================================\n";
        std::cout << "Processing file: " << filename << "\n";

        // 파일 처리 (내부 데이터 출력 없이 Chunk 수 계산)
        long long chunkCount = processFile(filename);

        // 결과 출력
        std::cout << " => Total 1KB chunks used = " << chunkCount << "\n";
        std::cout << " => Total usage in bytes  = " << (chunkCount * 1024LL) << " bytes\n";
        std::cout << " => Total usage in KB     = " << chunkCount << " KB\n";
    }

    std::cout << "=====================================\n";
    std::cout << "Done.\n";
    std::cout << "=====================================\n";

    return 0;
}

