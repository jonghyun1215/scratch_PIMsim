#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tuple>
#include <algorithm>
#include <string>

// Function to process a single .mtx file
void processMtxFile(const std::string& inputFileName, const std::string& outputFileName) {
    std::ifstream inputFile(inputFileName);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open file: " << inputFileName << std::endl;
        return;
    }

    std::ofstream outputFile(outputFileName);
    if (!outputFile.is_open()) {
        std::cerr << "Failed to create file: " << outputFileName << std::endl;
        return;
    }

    std::string line;
    std::vector<std::tuple<int, int, double>> data;
    bool isHeader = true;

    while (std::getline(inputFile, line)) {
        // Skip comments and headers
        if (line[0] == '%' || isHeader) {
            if (line[0] != '%') isHeader = false;
            outputFile << line << "\n";
            continue;
        }

        std::istringstream iss(line);
        int row, col;
        double value;
        if (!(iss >> row >> col)) {
            std::cerr << "Malformed line in " << inputFileName << ": " << line << std::endl;
            continue;
        }

        // If NNZ is empty, default to 1
        if (!(iss >> value)) {
            value = 1.0;
        }

        // Ignore entries with value == 0
        if (value == 0.0) {
            data.emplace_back(row, col, 1.0);
        }
		else
			data.emplace_back(row, col, value);
    }

    // Sort by column-major order
    std::sort(data.begin(), data.end(), [](const auto& a, const auto& b) {
        return std::get<1>(a) < std::get<1>(b) || (std::get<1>(a) == std::get<1>(b) && std::get<0>(a) < std::get<0>(b));
    });

    // Write sorted data to output file
    for (const auto& [row, col, value] : data) {
        outputFile << row << " " << col << " " << value << "\n";
    }

    inputFile.close();
    outputFile.close();
    std::cout << "Processed: " << inputFileName << " -> " << outputFileName << std::endl;
}

int main() {
    std::vector<std::string> fileNames = {
        // "Stanford.mtx", "G2_circuit.mtx", "bcsstk32.mtx", "cant.mtx", "consph.mtx",
        // "crankseg_2.mtx", "ct20stif.mtx", "lhr71.mtx", "ohne2.mtx", "pdb1HYS.mtx",
        // "pwtk.mtx", "rma10.mtx", "shipsec1.mtx", "soc-sign-epinions.mtx",
        // "sorted_consph.mtx", "webbase-1M.mtx", "xenon2.mtx", "ASIC_100k.mtx"
        "cora.mtx",
        "citeseer.mtx",
        "amazon-photo.mtx",
        "amazon-com.mtx",
        "Pubmed.mtx",
        "corafull.mtx",
        "coauthor-phy.mtx",
        "coauthor-cs.mtx",
        "cornell.mtx",
        "chameleon.mtx",
        "squirrel.mtx"
    };

    for (const auto& fileName : fileNames) {
        std::string outputFileName = "sorted_suite/"+ fileName.substr(0, fileName.find_last_of('.')) + ".mtx";
        processMtxFile(fileName, outputFileName);
    }

    return 0;
}

