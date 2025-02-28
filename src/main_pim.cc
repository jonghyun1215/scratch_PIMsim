#include <iostream>
#include <random>
#include "./../ext/headers/args.hxx"
#include "./transaction_generator.h"
#include "half.hpp"

using namespace dramsim3;
using half_float::half;

int main(int argc, const char **argv) {
    srand(time(NULL));

    // parse simulation settings
    args::ArgumentParser parser(
        "PIM-DRAM Simulator.",
        "Examples:\n"
        "./build/pimdramsim3main configs/DDR4_8Gb_x8_3200.ini -c 100 -t sample_trace.txt -m cant -w\n"
        "./build/pimdramsim3main configs/DDR4_8Gb_x8_3200.ini -s random -c 100 -m bcsstk32");
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
        parser, "pim_api", "PIM API - spmv, nopim_spmv",
        {"pim-api"}, "add");
    args::ValueFlag<std::string> matrix_base_arg(
        parser, "matrix_base", "Matrix base name (e.g., cant, bcsstk32)", {'m', "matrix"}, "cant");
    args::Flag sw_opt_flag(parser, "sw_opt", "Enable SW_OPT", {'w', "sw-opt"});

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
    std::string matrix_base = args::get(matrix_base_arg);
    bool sw_opt = args::get(sw_opt_flag);

    // 생성할 파일 경로 구성
    std::string mtx_filename = "../sparse_suite/suite/" + matrix_base + ".mtx";
    std::string dat_filename = sw_opt 
        ? "../sparse_suite/draf_dat/tiled_draf_" + matrix_base + ".dat"
        : "../sparse_suite/wo_sw_opt_dat/" + matrix_base + ".dat";
    
    // Initialize modules of PIM-Simulator
    std::cout << C_GREEN << "Initializing modules..." << C_NORMAL << std::endl;
    TransactionGenerator * tx_generator;

    std::random_device random_device;
    auto rng = std::mt19937(random_device());
    auto f32rng = std::bind(std::normal_distribution<float>(0, 1), std::ref(rng));

    if(pim_api == "spmv") {
        std::vector<std::vector<re_aligned_dram_format>> DRAF_BG;
        DRAF_BG = loadResultFromFile(dat_filename, 64);
        
        COOMatrixInfo matrix = readMTXFileInformation(mtx_filename);
        int m = matrix.n_rows;
        uint8_t *output_vector = (uint8_t *) malloc(sizeof(uint16_t) * m);

        tx_generator = new SpmvTransactionGenerator(config_file, output_dir,
                                                    DRAF_BG, output_vector);
    }
    else if(pim_api == "nopim_spmv") {
        std::vector<std::vector<re_aligned_dram_format>> DRAF_BG = loadResultFromFile(dat_filename, 64);
        
        COOMatrixInfo matrix = readMTXFileInformation(mtx_filename);
        int m = matrix.n_rows;
        uint8_t *output_vector = (uint8_t *) malloc(sizeof(uint16_t) * m);

        tx_generator = new NoPIMSpmvTransactionGenerator(config_file, output_dir,
                                                    DRAF_BG, output_vector);
    }

    std::cout << C_GREEN << "Success Module Initialize" << C_NORMAL << "\n\n";

    uint64_t clk;
    std::cout << C_GREEN << "Initializing severals..." << C_NORMAL << std::endl;
    clk = tx_generator->GetClk();
    tx_generator->Initialize();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success Initialize (" << clk << " cycles)" << C_NORMAL << "\n\n";

    std::cout << C_GREEN << "Setting Data..." << C_NORMAL << "\n";
    clk = tx_generator->GetClk();
    tx_generator->SetData();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success SetData (" << clk << " cycles)" << C_NORMAL << "\n\n";

    std::cout << C_GREEN << "Executing..." << C_NORMAL << "\n";
    tx_generator->is_print_ = true;
    clk = tx_generator->GetClk();
    tx_generator->start_clk_ = clk;
    tx_generator->Execute();
    clk = tx_generator->GetClk() - clk;
    tx_generator->is_print_ = false;
    std::cout << C_GREEN << "Success Execute (" << clk << " cycles)" << C_NORMAL << "\n\n";

    std::cout << C_GREEN << "Getting Result..." << C_NORMAL << "\n";
    clk = tx_generator->GetClk();
    tx_generator->GetResult();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success GetResult (" << clk << " cycles)" << C_NORMAL << "\n\n";

    std::cout << C_GREEN << "Additional Accumulating Result..." << C_NORMAL << "\n";
    clk = tx_generator->GetClk();
    tx_generator->AdditionalAccumulation();
    clk = tx_generator->GetClk() - clk;
    std::cout << C_GREEN << "Success Accumulation (" << clk << " cycles)" << C_NORMAL << "\n\n";

    tx_generator->CheckResult();
    tx_generator->PrintStats();
    delete tx_generator;

    return 0;
}
