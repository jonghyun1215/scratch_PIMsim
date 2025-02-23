#ifndef __TRANSACTION_GENERATOR_H
#define __TRANSACTION_GENERATOR_H

#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <string>
#include <cstdint>
#include "./memory_system.h"
#include "./configuration.h"
#include "./common.h"
#include "./pim_config.h"
#include "./half.hpp"
#include "../sparse_suite/sw_full_stack.h"

//#include "./../coo_partitioned/spmvArrCSC.h"

#define EVEN_BANK 0
#define ODD_BANK  1

#define NUM_WORD_PER_ROW     32
#define NUM_UNIT_PER_WORD    16
#define NUM_CHANNEL          16 //TW added
#define NUM_BANK_PER_CHANNEL 16
#define NUM_BANK             (NUM_BANK_PER_CHANNEL * NUM_CHANNEL) //16 * 16 = 256
#define SIZE_WORD            32
#define SIZE_ROW             (SIZE_WORD * NUM_WORD_PER_ROW) //32 * 32 = 1024

// Mapping을 위한 주소들에 대한 정의
#define MAP_SBMR             0x3fff //Single Bank Mode Register
#define MAP_ABMR             0x3ffe //All Bank Mode Register
#define MAP_PIM_OP_MODE      0x3ffd
#define MAP_CRF              0x3ffc
#define MAP_GRF              0x3ffb
#define MAP_SRF              0x3ffa
// TW added
// 0x3ff9 is reserved for global accumulator
#define TRIGGER_GACC       0x3ff9

#define C_NORMAL "\033[0m"
#define C_RED    "\033[031m"
#define C_GREEN  "\033[032m"
#define C_YELLOW "\033[033m"
#define C_BLUE   "\033[034m"

namespace dramsim3 {

class TransactionGenerator {
 public:
    TransactionGenerator(const std::string& config_file,
                         const::string& output_dir)
        : memory_system_(
              config_file, output_dir,
              std::bind(&TransactionGenerator::ReadCallBack, this,
                        std::placeholders::_1, std::placeholders::_2),
              std::bind(&TransactionGenerator::WriteCallBack, this,
                        std::placeholders::_1)),
          config_(new Config(config_file, output_dir)),
          clk_(0) {
        pmemAddr_size_ = (uint64_t)4 * 1024 * 1024 * 1024;
        pmemAddr_ = (uint8_t *) mmap(NULL, pmemAddr_size_,
                                     PROT_READ | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE,
                                     -1, 0);
        if (pmemAddr_ == (uint8_t*) MAP_FAILED)
            perror("mmap");
        burstSize_ = 32; // 32B

        data_temp_ = (uint8_t *) malloc(burstSize_);

        memory_system_.init(pmemAddr_, pmemAddr_size_, burstSize_);

        is_print_ = false;
        start_clk_ = 0;
        cnt_ = 0;
    }
    ~TransactionGenerator() { delete(config_); }
    // virtual void ClockTick() = 0;
    virtual void Initialize() = 0;
    virtual void SetData() = 0;
    virtual void Execute() = 0;
    virtual void GetResult() = 0;
    virtual void CheckResult() = 0;
    virtual void AdditionalAccumulation() = 0;

   //아래에 있는 Function들은 모든 transacion generator에서 사용할 수 있는 함수들
   //위의 함수는 override를 하여, 사용하는 경우의 transaction generator가 각각 정의
    void ReadCallBack(uint64_t addr, uint8_t *DataPtr);
    void WriteCallBack(uint64_t addr);
    void PrintStats() { memory_system_.PrintStats(); }
    uint64_t ReverseAddressMapping(Address& addr);
    uint64_t Ceiling(uint64_t num, uint64_t stride);
    void TryAddTransaction(uint64_t hex_addr, bool is_write, uint8_t *DataPtr);
    void Barrier();
	uint64_t GetClk() { return clk_; }

    bool is_print_;
    uint64_t start_clk_;
    int cnt_;

 protected:
    MemorySystem memory_system_;
    const Config *config_;
    uint8_t *pmemAddr_;
    uint64_t pmemAddr_size_;
    unsigned int burstSize_;
    uint64_t clk_;

    uint8_t *data_temp_;
};

//TW added
class SpmvTransactionGenerator : public TransactionGenerator {
 public:
    SpmvTransactionGenerator(const std::string& config_file,
                             const std::string& output_dir,
                             std::vector<std::vector<re_aligned_dram_format>> DRAF_BG,
                             uint8_t *output_vector)
        : TransactionGenerator(config_file, output_dir),
          DRAF_BG_(DRAF_BG), output_vector_(output_vector){}
    void Initialize() override;
    void SetData() override;
    void Execute() override;
    void GetResult() override;
    void AdditionalAccumulation() override;
    void CheckResult() override {};

    uint8_t *partial_index_;
    uint8_t *partial_value_;

 private:
    void ExecuteBank(int bank);

    std::vector<std::vector<re_aligned_dram_format>> DRAF_BG_;
    uint8_t *output_vector_; 
    uint32_t kernel_execution_time_;
    //uint64_t m_, n_; //Matrix의 크기를 전달하기 위한 코드
    uint64_t addr_DRAF_, addr_output_vector_;
    uint64_t ukernel_access_size_;
    uint64_t ukernel_count_per_pim_;
    uint32_t *ukernel_spmv_;
    uint32_t *ukernel_spmv_last_;
};

//TW added
class NoPIMSpmvTransactionGenerator : public TransactionGenerator {
   public:
      NoPIMSpmvTransactionGenerator(const std::string& config_file,
                               const std::string& output_dir,
                               std::vector<std::vector<re_aligned_dram_format>> DRAF_BG,
                               uint8_t *output_vector)
          : TransactionGenerator(config_file, output_dir),
            DRAF_BG_(DRAF_BG), output_vector_(output_vector){}
      void Initialize() override;
      void SetData() override;
      void Execute() override{};
      void GetResult() override;
      void AdditionalAccumulation() override {};
      void CheckResult() override {};
  
      uint8_t *partial_index_;
      uint8_t *partial_value_;
  
   private:
      void ExecuteBank(int bank);
  
      std::vector<std::vector<re_aligned_dram_format>> DRAF_BG_;
      uint8_t *output_vector_; 
      uint32_t kernel_execution_time_;
      //uint64_t m_, n_; //Matrix의 크기를 전달하기 위한 코드
      uint64_t addr_DRAF_, addr_output_vector_;
      uint64_t ukernel_access_size_;
      uint64_t ukernel_count_per_pim_;
      uint32_t *ukernel_spmv_;
      uint32_t *ukernel_spmv_last_;
  };
  

//TW added
class CPUSpmvTransactionGenerator : public TransactionGenerator {
 public:
    CPUSpmvTransactionGenerator(const std::string& config_file,
                             const std::string& output_dir,
                             uint32_t n_rows,
                             uint32_t n_cols, 
                             uint32_t nnz, 
                             double miss_ratio)
        : TransactionGenerator(config_file, output_dir),
          n_rows_(n_rows), n_cols_(n_cols), nnz_(nnz), miss_ratio_(miss_ratio) {}
    void Initialize() override;
    void SetData() override {};
    void Execute() override;
    void GetResult() override {};
    void CheckResult() override {};
    void AdditionalAccumulation() override{};

 private:
    void ExecuteBank(int bank, int batch);
    
    uint32_t n_rows_, n_cols_, nnz_;
    double miss_ratio_;
    uint32_t addr_row_indices_, addr_col_indices_, addr_val_; 
    uint32_t addr_x_, addr_y_;
    
   // Cache for column indices and vector x elements
    std::unordered_map<uint64_t, bool> cache_col_; // Cache to track accessed column indices
    std::unordered_map<uint64_t, uint16_t> cache_x_; // Cache for vector x elements (32B granularity)

};

//TW added end

}  // namespace dramsim3

#endif //__TRANSACTION_GENERATOR_H
