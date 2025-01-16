#include <iostream>
#include <random>
#include "./../ext/headers/args.hxx"
#include "./transaction_generator.h"
#include "half.hpp"


using namespace dramsim3;
using half_float::half;
//Main code for control PIM simulator

// main code to simulate PIM simulator
int main(int argc, const char **argv) {
    srand(time(NULL));

    // parse simulation settings
    args::ArgumentParser parser(
        "PIM-DRAM Simulator.",
        "Examples: \n."
        "./build/pimdramsim3main configs/DDR4_8Gb_x8_3200.ini -c 100 -t "
        "sample_trace.txt\n"
        "./build/pimdramsim3main configs/DDR4_8Gb_x8_3200.ini -s random -c 100");
    args::HelpFlag help(parser, "help", "Display the help menu", {'h', "help"});
    args::ValueFlag<uint64_t> num_cycles_arg(parser, "num_cycles",
                                             "Number of cycles to simulate",
                                             {'c', "cycles"}, 100000);
    args::ValueFlag<std::string> output_dir_arg(
        parser, "output_dir", "Output directory for stats files",
        {'o', "output-dir"}, ".");
    args::Positional<std::string> config_arg(
        parser, "config", "The config file name (mandatory)");
    args::ValueFlag<std::string> pim_api_arg(
        parser, "pim_api", "PIM API - spmv",
        {"pim-api"}, "add"); //TW addded
    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    std::string config_file = args::get(config_arg);
    if (config_file.empty()) {
        std::cerr << parser;
        return 1;
    }

    uint64_t cycles = args::get(num_cycles_arg);
    std::string output_dir = args::get(output_dir_arg);
    std::string pim_api = args::get(pim_api_arg);

    // Initialize modules of PIM-Simulator
    //  Transaction Generator + DRAMsim3 + PIM Functional Simulator
    std::cout << C_GREEN << "Initializing modules..." << C_NORMAL << std::endl;
    TransactionGenerator * tx_generator;

    // Define operands and Transaction generator for simulating computation
	std::random_device random_device;
	auto rng = std::mt19937(random_device());
	auto f32rng = std::bind(std::normal_distribution<float>(0, 1), std::ref(rng));
    
    //TW added
    if(pim_api == "spmv"){ //64개로 그냥 tiled 된 결과를 불러와 실행
        //여기서 데이터를 물어와야 될 듯
        //만들어 놓은 데이터를 불러오는 곳

        //DRAF must be initialized from header file (or dat file)
        std::vector<std::vector<re_aligned_dram_format>> DRAF_BG= loadResultFromFile("../sparse_suite/tiled_draf.dat",64);

        // Calculate total size needed for the DRAF array
        size_t totalSize = 0;
        for (const auto& bg : DRAF_BG) {
            totalSize += bg.size() * sizeof(re_aligned_dram_format);
        }

        //(TODO) 여기서 Byte 형태로 바꿀지, 아니면 SpmvTransactionGenerator에서 바꿀지 결정
        //우선은 바꿔서 넘겨주는 것으로 구현
        uint8_t* DRAF = static_cast<uint8_t*>(malloc(totalSize));

        uint8_t* currentPtr = DRAF; // Preserve base pointer

        // Flatten DRAF_BG into DRAF
        /*for (const auto& bg : DRAF_BG) {
            for (const auto& element : bg) {
                std::memcpy(currentPtr, &element, sizeof(re_aligned_dram_format));
                currentPtr += sizeof(re_aligned_dram_format);
            }
        }*/
        // DRAF now contains all elements of DRAF_BG in byte format

        //Output vector
        // (TODO) m을 어떻게 받아올지 결정
        // (TODO) output vector의 길이를 사전에 결정하여, 전달해 주는 방식?
        COOMatrixInfo matrix = readMTXFileInformation("../sparse_suite/suite/cant.mtx");

        int m = matrix.n_rows; // m must be initialized from header file
        uint8_t *output_vector = (uint8_t *) malloc(sizeof(uint16_t) * m);

        // Define Transaction generator for GEMV computation
        //tx_generator = new SpmvTransactionGenerator(config_file, output_dir,
        //                                            DRAF, output_vector);
        tx_generator = new SpmvTransactionGenerator(config_file, output_dir,
                                                    DRAF_BG, output_vector);
    }
    //TW added end
    
    std::cout << C_GREEN << "Success Module Initialize" << C_NORMAL << "\n\n";

	uint64_t clk;

    // Initialize variables and ukernel
    std::cout << C_GREEN << "Initializing severals..." << C_NORMAL << std::endl;
	clk = tx_generator->GetClk();
    tx_generator->Initialize();
	clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success Initialize (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Write operand data and μkernel to physical memory and PIM registers
    std::cout << C_GREEN << "Setting Data..." << C_NORMAL << "\n";
	clk = tx_generator->GetClk();
    tx_generator->SetData();
	clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success SetData (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Execute PIM computation
    std::cout << C_GREEN << "Executing..." << C_NORMAL << "\n";
    tx_generator->is_print_ = true;
	clk = tx_generator->GetClk();
    tx_generator->start_clk_ = clk;
    tx_generator->Execute();
	clk = tx_generator->GetClk() - clk;
    tx_generator->is_print_ = false;
    std::cout << C_GREEN << "Success Execute (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Read PIM computation result from physical memory
    std::cout << C_GREEN << "Getting Result..." << C_NORMAL << "\n";
	clk = tx_generator->GetClk();
    tx_generator->GetResult();
	clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success GetResult (" << clk << " cycles)" << C_NORMAL << "\n\n";

    // Calculate error between the result of PIM computation and actual answer
    tx_generator->CheckResult();

    tx_generator->PrintStats();

    delete tx_generator;

    return 0;
}
