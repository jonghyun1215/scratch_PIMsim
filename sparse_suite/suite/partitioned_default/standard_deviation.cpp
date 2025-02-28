#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>    // sqrt

/**
 * @brief 파일이 열리는지만 확인
 */
bool fileExists(const std::string &filename) {
    std::ifstream fin(filename);
    return fin.is_open();
}

/**
 * @brief 해당 .mtx 파일 헤더에서 nnz(비제로 원소 개수)만 읽어오는 함수
 */
int readMtxNNZOnly(const std::string &filename)
{
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::string line;
    // '%'로 시작하는 주석 라인 스킵
    while (true) {
        if (!std::getline(fin, line)) {
            throw std::runtime_error("Invalid MTX file format in: " + filename);
        }
        if (!line.empty() && line[0] != '%') {
            // 주석이 아닌 첫 줄 (nrows ncols nnz) 정보를 담고 있다고 가정
            break;
        }
    }

    int nrows, ncols, nnz;
    {
        std::stringstream ss(line);
        ss >> nrows >> ncols >> nnz;
    }

    fin.close();
    return nnz;
}

/**
 * @brief 이미 만들어져 있는 파티션 파일들(partition_0.mtx ~ partition_63.mtx)을
 *        다시 열어서, NNZ의 표준편차를 계산한다.
 *
 * @param partitionDir  파티션 파일이 있는 디렉토리 경로
 *                      (예: "tile_wo_optimization/Stanford_tiled")
 * @param matrixName    해당 매트릭스 이름 (출력 용도)
 */
void computeStdDevOfPartitions(const std::string &partitionDir,
                               const std::string &matrixName)
{
    const int numPartitions = 64;
    std::vector<int> nnzCounts(numPartitions);
    double sumNnz = 0.0;

    // partition_0.mtx ~ partition_63.mtx 파일의 nnz를 헤더에서 읽음
    for (int p = 0; p < numPartitions; p++) {
        // 예: "tile_wo_optimization/Stanford_tiled/partition_0.mtx"
        std::string partFile = partitionDir + "/partition_" + std::to_string(p) + ".mtx";

        if (!fileExists(partFile)) {
            // 파일이 없다면 오류 혹은 0으로 처리
            std::cerr << "[Warning] Partition file missing: " << partFile << "\n";
            nnzCounts[p] = 0;
            continue;
        }

        try {
            int nnz = readMtxNNZOnly(partFile);
            nnzCounts[p] = nnz;
            sumNnz += nnz;
        } catch (const std::exception &ex) {
            std::cerr << "[Error] " << ex.what() << "\n";
            nnzCounts[p] = 0;
        }
    }

    // 평균
    double mean = sumNnz / numPartitions;

    // 분산
    double varSum = 0.0;
    for (auto c : nnzCounts) {
        double diff = c - mean;
        varSum += diff * diff;
    }
    double variance = varSum / numPartitions;
    double stddev = std::sqrt(variance);

    // 출력
    std::cout << "======================================\n";
    std::cout << "[Matrix: " << matrixName << "]\n"
              << " - Partition Directory: " << partitionDir << "\n"
              << " - #Partitions: " << numPartitions << "\n"
              << " - Average NNZ per partition: " << mean << "\n"
              << " - Standard deviation of NNZ: " << stddev << "\n";
    std::cout << "======================================\n\n";
}

int main()
{
    // 이미 만들어진 파티션 디렉토리(매트릭스별) 목록
    // 예를 들어, "Stanford"라면
    // "tile_wo_optimization/Stanford_tiled/partition_0.mtx" ~ partition_63.mtx 이 존재한다고 가정
    std::vector<std::string> matrixNames = {
        "ASIC_100k",
        "bcsstk32",
        "cant",
        "consph",
        "crankseg_2",
        "ct20stif",
        "lhr71",
        "ohne2",
        "pdb1HYS",
        "pwtk",
        "rma10",
        "shipsec1",
        "soc-sign-epinions",
        "Stanford",
        "webbase-1M",
        "xenon2"
    };

    for (auto &name : matrixNames) {
        std::string partitionDir = name + "_new";
        computeStdDevOfPartitions(partitionDir, name);
    }

    return 0;
}

