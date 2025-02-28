#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>        // std::sqrt
#include <sys/stat.h>   // stat(), mkdir() 등
#include <sys/types.h>  // mode_t 등
#include <cerrno>
#include <cstring>

/**
 * @brief 하나의 원소(행, 열, 값)를 저장하기 위한 구조체
 */
struct Triplet {
    int row;
    int col;
    double val;
};

/**
 * @brief 파일 존재 여부 확인 (단순히 열어지는지로 판단)
 */
bool fileExists(const std::string &filename) {
    std::ifstream fin(filename);
    return fin.is_open();
}

/**
 * @brief 디렉토리가 이미 존재하는지 여부 확인
 */
bool dirExists(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

/**
 * @brief 리눅스/유닉스 계열에서 "mkdir -p"와 유사하게 하위 디렉토리까지 생성
 * @param path 생성할 디렉토리 경로 (예: "tile_wo_optimization/Stanford_tiled")
 * @return 성공 시 true, 실패 시 false
 */
bool createDirectories(const std::string &path) {
    if (path.empty()) {
        return false;
    }

    std::string dirPath = path;
    // 경로 맨 끝에 '/'가 있다면 제거
    if (!dirPath.empty() && dirPath.back() == '/') {
        dirPath.pop_back();
    }

    // 루트부터 차근차근 디렉토리를 만든다.
    // 예: "tile_wo_optimization/Stanford_tiled"
    //   -> "tile_wo_optimization" 만들고,
    //   -> "tile_wo_optimization/Stanford_tiled" 생성
    size_t pos = 0;
    do {
        pos = dirPath.find('/', pos + 1);
        std::string sub = dirPath.substr(0, pos);

        if (!sub.empty() && !dirExists(sub)) {
            if (mkdir(sub.c_str(), 0755) != 0) {
                if (errno != EEXIST) {
                    std::cerr << "Failed to create directory: " << sub
                              << " (" << strerror(errno) << ")\n";
                    return false;
                }
            }
        }
    } while (pos != std::string::npos);

    return true;
}

/**
 * @brief MatrixMarket 파일을 읽어 COO (row, col, value) 형태로 파싱
 *
 * @param filename 읽을 MTX 파일 이름
 * @param nrows    행 수 (출력 변수)
 * @param ncols    열 수 (출력 변수)
 * @param nnz      비제로(Non-zero) 원소 개수 (출력 변수)
 * @return         원소(Triplet)들의 벡터
 */
std::vector<Triplet> readMtxFile(const std::string &filename,
                                 int &nrows, int &ncols, int &nnz)
{
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::string line;
    // MatrixMarket 헤더/주석 부분 무시
    while (true) {
        if (!std::getline(fin, line)) {
            throw std::runtime_error("Invalid MTX file format.");
        }
        // '%'로 시작하는 줄은 주석, 처음으로 주석 아닌 줄을 만나면 break
        if (!line.empty() && line[0] != '%') {
            break;
        }
    }

    {
        // 첫 번째 유효한 라인에서 행, 열, nnz 정보 파싱
        std::stringstream ss(line);
        ss >> nrows >> ncols >> nnz;
    }

    std::vector<Triplet> triplets;
    triplets.reserve(nnz);

    // (row, col, value) 데이터 읽기 (1-based 인덱스 가정)
    for (int i = 0; i < nnz; i++) {
        int r, c;
        double v;
        fin >> r >> c >> v;
        triplets.push_back({r, c, v});
    }

    fin.close();
    return triplets;
}

/**
 * @brief 64개 파티션으로 나누어(열 기준) 각 파티션의 Triplet 목록을 반환
 *
 * @param triplets   전체 원소 리스트
 * @param nrows      전체 행 수 (현재 코드에서는 사용 안 하지만, 필요시 활용 가능)
 * @param ncols      전체 열 수
 * @return           파티션별 Triplet 벡터 (총 64개)
 */
std::vector<std::vector<Triplet>> partitionByColumn64(const std::vector<Triplet> &triplets,
                                                      int nrows, int ncols)
{
    int numPartitions = 64;
    // 파티션별로 열 개수를 최대한 균등하게 분배
    // 예: ncols = 1000이라면, 각 파티션은 15~16 열씩
    int baseSize = ncols / numPartitions;
    int remainder = ncols % numPartitions;

    // 파티션별 열 범위 [startCol, endCol)
    std::vector<int> boundary(numPartitions + 1, 0);
    int currentColStart = 1;
    for (int p = 0; p < numPartitions; p++) {
        boundary[p] = currentColStart;
        int partitionSize = baseSize + ((p < remainder) ? 1 : 0);
        currentColStart += partitionSize;
    }
    boundary[numPartitions] = ncols + 1; // 마지막 파티션의 끝

    // 파티션 결과 저장용
    std::vector<std::vector<Triplet>> partitionedTriplets(numPartitions);

    // 각 (row, col, val)에 대해 소속 파티션을 찾는다
    for (const auto &t : triplets) {
        for (int p = 0; p < numPartitions; p++) {
            if (t.col >= boundary[p] && t.col < boundary[p + 1]) {
                // 파티션 p에 삽입
                // (원본 인덱스를 그대로 유지)
                partitionedTriplets[p].push_back({t.row, t.col, t.val});
                break;
            }
        }
    }

    return partitionedTriplets;
}
/*
std::vector<std::vector<Triplet>> partitionByColumn64(const std::vector<Triplet> &triplets,
                                                      int nrows, int ncols)
{
    const int numPartitions = 64;
    std::vector<std::vector<Triplet>> partitionedTriplets(numPartitions);

    // 각 Triplet에 대해 열 번호에 따라 청크 인덱스를 결정 (round-robin)
    for (const auto &t : triplets) {
        int partitionIndex = t.col % numPartitions;
        partitionedTriplets[partitionIndex].push_back({t.row, t.col, t.val});
    }

    return partitionedTriplets;
}
*/

/**
 * @brief 파티션별 Triplet 정보를 MatrixMarket(coordinate) 형식으로 파일에 저장
 *
 * @details
 * - 파티션 헤더에 (maxRow, maxCol, nnz)를 적는다.
 * - (row, col, val)은 원본 인덱스 그대로 유지.
 *
 * @param partitionTriplets 파티션별 Triplet 목록
 * @param outDir            출력될 디렉토리 경로
 */
void writePartitionedMtx(const std::vector<std::vector<Triplet>> &partitionTriplets,
                         const std::string &outDir)
{
    // "mkdir -p" 기능 대체
    if (!createDirectories(outDir)) {
        std::cerr << "Failed to create output directory: " << outDir << "\n";
        return;
    }

    int numPartitions = static_cast<int>(partitionTriplets.size());

    // 파티션별로 파일 생성
    for (int p = 0; p < numPartitions; p++) {
        std::string outFile = outDir + "/partition_" + std::to_string(p) + ".mtx";
        std::ofstream fout(outFile);
        if (!fout.is_open()) {
            std::cerr << "Cannot open output file: " << outFile << "\n";
            continue;
        }

        const auto &part = partitionTriplets[p];
        int localNnz = static_cast<int>(part.size());

        // localNnz가 0이면 maxRow, maxCol도 0으로 설정
        int maxRow = 0, maxCol = 0;
        for (auto &t : part) {
            if (t.row > maxRow) maxRow = t.row;
            if (t.col > maxCol) maxCol = t.col;
        }

        // MatrixMarket 헤더
        fout << "%%MatrixMarket matrix coordinate real general\n";
        fout << maxRow << " " << maxCol << " " << localNnz << "\n";

        // (row, col, val) 출력
        for (auto &t : part) {
            fout << t.row << " " << t.col << " " << t.val << "\n";
        }

        fout.close();
    }
}

/**
 * @brief 특정 MTX 파일을 처리하여 64개 파티션으로 분할 후 저장, 파티션 NNZ 표준편차 계산
 *
 * @param filename  MTX 파일 이름 (예: "Stanford.mtx")
 */
void processMatrixFile(const std::string &filename)
{
    // 1. MTX 파일 읽기
    int nrows, ncols, nnz;
    std::vector<Triplet> triplets = readMtxFile(filename, nrows, ncols, nnz);

    // 2. 열 기준으로 64개 파티션
    auto partitioned = partitionByColumn64(triplets, nrows, ncols);

    // 2-1. 파티션별 NNZ(= partition.size())를 통해 표준편차 계산
    {
        // 64개 파티션 NNZ를 저장
        std::vector<int> nnzCounts(partitioned.size());
        double sumNnz = 0.0;
        for (size_t i = 0; i < partitioned.size(); i++) {
            nnzCounts[i] = static_cast<int>(partitioned[i].size());
            sumNnz += nnzCounts[i];
        }

        double mean = sumNnz / partitioned.size(); // 평균

        double varSum = 0.0;
        for (auto count : nnzCounts) {
            double diff = count - mean;
            varSum += diff * diff;
        }
        double variance = varSum / partitioned.size();   // 분산
        double stddev = std::sqrt(variance);             // 표준편차

        std::cout << "[Matrix: " << filename << "]\n"
                  << " -> #Partitions: " << partitioned.size() << "\n"
                  << " -> Average NNZ per partition: " << mean << "\n"
                  << " -> Standard deviation of NNZ: " << stddev << "\n\n";
    }

    // 3. 결과물 저장 경로 결정
    //    예: "Stanford.mtx" -> baseName = "Stanford"
    //        결과 폴더: "tile_wo_optimization/Stanford_tiled"
    std::string baseName = filename;
    {
        auto pos = baseName.rfind(".mtx");
        if (pos != std::string::npos) {
            baseName.erase(pos);
        }
    }
    std::string outDir = "tile_wo_optimization/" + baseName + "_tiled";

    // 4. 파티션 결과 파일 쓰기
    writePartitionedMtx(partitioned, outDir);
}

int main()
{
    // 예시: 문제에서 제시된 파일들
    std::vector<std::string> files = {
        "ASIC_100k.mtx",
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
        "Stanford.mtx",
        "webbase-1M.mtx",
        "xenon2.mtx"
    };

    // 파일 존재 여부 확인 후 처리
    for (auto &f : files) {
        if (fileExists(f)) {
            try {
                processMatrixFile(f);
            } catch (const std::exception &ex) {
                std::cerr << "Error processing " << f << ": " << ex.what() << std::endl;
            }
        } else {
            std::cerr << "File does not exist: " << f << std::endl;
        }
    }

    return 0;
}

