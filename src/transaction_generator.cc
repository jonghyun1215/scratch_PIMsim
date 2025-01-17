#include "transaction_generator.h"
#include <iostream>
#include <unordered_map>

using half_float::half;

namespace dramsim3 {

void TransactionGenerator::ReadCallBack(uint64_t addr, uint8_t *DataPtr) {
    return;
}
void TransactionGenerator::WriteCallBack(uint64_t addr) {
    return;
}

// Map 64-bit hex_address into structured address
//HBM2_4Gb_test.ini 파일에 정의된 Address Mapping을 이용하여 hex_addr을 Address로 변환
//Address는 Row, Rank, Column, Bankgroup, Bank, Channel로 구성
uint64_t TransactionGenerator::ReverseAddressMapping(Address& addr) {
    uint64_t hex_addr = 0;
    hex_addr += ((uint64_t)addr.channel) << config_->ch_pos;
    hex_addr += ((uint64_t)addr.rank) << config_->ra_pos;
    hex_addr += ((uint64_t)addr.bankgroup) << config_->bg_pos;
    hex_addr += ((uint64_t)addr.bank) << config_->ba_pos;
    hex_addr += ((uint64_t)addr.row) << config_->ro_pos;
    hex_addr += ((uint64_t)addr.column) << config_->co_pos;
    return hex_addr << config_->shift_bits;
}

// Returns the minimum multiple of stride that is higher than num
uint64_t TransactionGenerator::Ceiling(uint64_t num, uint64_t stride) {
    std::cout << "num : " << num << ", stride : " << stride << \
    " return : " << ((num + stride - 1) / stride) * stride << std::endl;
    return ((num + stride - 1) / stride) * stride;
}

// Send transaction to memory_system (DRAMsim3 + PIM Functional Simulator)
//  hex_addr : address to RD/WR from physical memory or change bank mode
//  is_write : denotes to Read or Write
//  *DataPtr : buffer used for both RD/WR transaction (read common.h)
void TransactionGenerator::TryAddTransaction(uint64_t hex_addr, bool is_write,
                                             uint8_t *DataPtr) {
    // Wait until memory_system is ready to get Transaction
    while (!memory_system_.WillAcceptTransaction(hex_addr, is_write)) {
        memory_system_.ClockTick();
        clk_++;
    }
    // Send transaction to memory_system
    if (is_write) {
        //burstSize_ = 32B
        uint8_t *new_data = (uint8_t *) malloc(burstSize_);
        std::memcpy(new_data, DataPtr, burstSize_);
	    //std::cout << std::hex << clk_ << "\twrite\t" << hex_addr << std::dec << std::endl;
        memory_system_.AddTransaction(hex_addr, is_write, new_data);
        memory_system_.ClockTick();
        clk_++;
    } else {
		//std::cout << std::hex << clk_ << "\tread\t" << hex_addr << std::dec << std::endl;
        memory_system_.AddTransaction(hex_addr, is_write, DataPtr);
        memory_system_.ClockTick();
        clk_++;
    }

    #if 0
    if(is_write)
	    std::cout << std::hex << cnt_ << "\twrite\t" << hex_addr << std::dec << std::endl;
    else
		std::cout << std::hex << cnt_ << "\tread\t" << hex_addr << std::dec << std::endl;
    cnt_++;
    #endif

    #if 0
    if (is_print_) {
        Address addr = config_->AddressMapping(hex_addr);
        if(addr.channel == 0 && (addr.bank == 0 || addr.bank == 1))
            std::cout << clk_-start_clk_ << "\t" << std::hex << hex_addr + 0x5000 << std::dec << std::endl;
    }
    #endif
    
}

// Prevent turning out of order between transaction parts
//  Change memory's threshold and wait until all pending transactions are
//  executed
void TransactionGenerator::Barrier() {
    //return;
    memory_system_.SetWriteBufferThreshold(0);
    while (memory_system_.IsPendingTransaction()) {
        memory_system_.ClockTick();
        clk_++;
    }
    memory_system_.SetWriteBufferThreshold(-1);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
//////////////////////TW added//////////////////////////////////
////////////////////////////////////////////////////////////////
void SpmvTransactionGenerator::Initialize() { //여기는 코딩 끝
    //TODO: ukernel_count_per_pim_ 계산을 어떻게 진행하는 것인지 확인
    std::cout<<"Initialize SpmvTransactionGenerator" << std::endl;
    addr_DRAF_ = 0; //Base address로 기본 pointing 수행

    //ukenrnel_access_size는 모두 SIZE_WORD * 8 * NUM_BANK로 동일
    //ukernel_access_size = Word(32) * 8 * # of Bank(256) = 65536
    ukernel_access_size_ = SIZE_WORD * 8 * NUM_BANK; // SIZE_WORD = 32, NUM_BANK = 256 (Channel 당 Bank 수는 16, 16채널)
    //UNIT_SIZE = 2, ukernel_access_size_ = 65536
    
    // (TODO) N을 어떻게 결정?
    //ukernel_count_per_pim_ = Ceiling(n_ * UNIT_SIZE, ukernel_access_size_)
    //std::cout<<"ukernel_count_per_pim_ : "<<ukernel_count_per_pim_<<std::endl;

    // Define ukernel for spmv
    ukernel_spmv_ = (uint32_t *) malloc(sizeof(uint32_t) * 32);

    // ukernel을 몇번 실행시킬지 결정하기 위해 추가한 코드
    // 가장 row를 많이 차지하는 DRAF_BG를 찾아서 그것을 기준으로 ukernel_count_per_pim_를 결정
    size_t max_size = 0;
    size_t max_index = 0;
    // Iterate over DRAF_BG to find the maximum size
    std::cout << "DRAF_BG_.size() : " << DRAF_BG_.size() << std::endl;

    for (uint32_t i = 0; i < DRAF_BG_.size(); ++i) {
        if (DRAF_BG_[i].size() > max_size) {
            max_size = DRAF_BG_[i].size();
            max_index = i;
        }
    }
    kernel_execution_time_ = DRAF_BG_[max_index].size(); // ukernel_count_per_pim_
    std::cout << "Max # of rows: " << kernel_execution_time_ << std::endl;
    
    //Odd bank
    ukernel_spmv_[0]=0b01001000000000001000000000000000;  // MOV(AAM0) SRF_M BANK
    ukernel_spmv_[1]=0b10010010001000001000000000000000;  // MUL(AAM0) GRF_A BANK SRF_M
    ukernel_spmv_[2]=0b11000000000000001000000000000000;  // SACC(AAM0) BANK BANK
    ukernel_spmv_[3] = 0b00010000000001000000100000000111; // JUMP       -1        7
    //ukernel_spmv_[3]=0b00010000000001000101000000001111;  // JUMP -2 7
    //Even bank
    ukernel_spmv_[4]=0b01001000000000001000000000000000;  // MOV(AAM0) SRF_M BANK
    ukernel_spmv_[5]=0b10010010001000001000000000000000;  // MUL(AAM0) GRF_A BANK SRF_M
    ukernel_spmv_[6]=0b11000000000000001000000000000000;  // SACC(AAM0) BANK BANK
    ukernel_spmv_[7]=0b00010000000001000101000000001111;  // JUMP -2 7
    //0b0001 0000 0000 0100 0101 0000 0000 1111
    // Exit
    // (TODO) MOV 명령어를 넣어 GRF에 있는 데이터를 위에 저장해 놓을지 결정
    ukernel_spmv_[8]=0b00100000000000000000000000000000;  // EXIT
}

//Memory에 PIM연산을 위한 데이터를 저장하는 과정
//ROW 하나당 23번의 write 필요
//column index 1번, value 7번, row index 14번, vector 1번 = 23번
void SpmvTransactionGenerator::SetData(){
    // strided size of one operand with one computation part(minimum)
    // UNIT_SIZE = 2, SIZE_WORD = 32, NUM_BANK = 256
    // strided_size = 2 * 32 * 256 = 16384
    
    //uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);

    #ifdef debug_mode
    std::cout << "HOST:\tSet input data...\n";
    #endif

    for (size_t i = 0; i < DRAF_BG_.size(); ++i) {
    uint32_t bg = i % 4;          // Fixed bg for each DRAF_BG[i]
    uint32_t current_row = 0;     // Start row at 0 for each DRAF_BG[i]
    uint32_t current_ch = i / 4;  // Increment ch after every 4 bg

        for (size_t j = 0; j < DRAF_BG_[i].size(); ++j) {
            uint32_t ba = j % 4;      // Bank cycles 0, 1, 2, 3
            if (ba == 0 && j > 0) {
                current_row++;        // Increment row after each full cycle of ba
            }

            uint32_t ro = current_row; // Row value
            uint32_t co = 0;          // Column (start of the row)

            // Construct the address
            Address addr(current_ch, 0, bg, ba, ro, co);

            // Translate to physical address
            uint64_t hex_addr = ReverseAddressMapping(addr);

            // Flatten re_aligned_dram_format into uint8_t*
            const re_aligned_dram_format& element = DRAF_BG_[i][j];
            const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&element);
            size_t total_size = sizeof(re_aligned_dram_format);

            for (co = 0; co < 32; co++) {
                Address addr(current_ch, 0, bg, ba, ro, co);
                hex_addr = ReverseAddressMapping(addr);
                if(co <= 21 || co ==29)
                    TryAddTransaction(hex_addr, true, const_cast<uint8_t*>(data_ptr + co*SIZE_WORD));
            }
        }
    }
    std::cout << "SetData Done" << std::endl;
    Barrier();

    // Mode transition: SB -> AB
    #ifdef debug_mode
    std::cout << "\nHOST:\t[1] SB -> AB \n";
    #endif
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        Address addr(ch, 0, 0, 0, MAP_ABMR, 0);
        uint64_t hex_addr = ReverseAddressMapping(addr);
        TryAddTransaction(hex_addr, false, data_temp_);
    }
    Barrier();

    // Program μkernel into CRF register
    #ifdef debug_mode
    std::cout << "\nHOST:\tProgram SpMV μkernel \n";
    #endif
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        for (int co = 0; co < 2; co++) { //for (int co = 0; co < 1; co++) {
            Address addr(ch, 0, 0, 0, MAP_CRF, co);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_spmv_[co*8]);
        }
    }
    Barrier();
}

void SpmvTransactionGenerator::Execute() {
    // NUM_WORD_PER_ROW = 32
    for (int ro = 0; ro < kernel_execution_time_; ro++) {
        for (int co_o = 0; co_o < NUM_WORD_PER_ROW / 8; co_o++) {
            // NUM_WORD_PER_ROW = 32, co_o = 0, 1, 2, 3
            // Mode transition: AB -> AB-PIM
            #ifdef debug_mode
            std::cout << "HOST:\t[2] AB -> PIM \n";
            #endif
            *data_temp_ |= 1;
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                //ch, rank, bankgroup, bank, row, column
                Address addr(ch, 0, 0, 0, MAP_PIM_OP_MODE, 0); // MAP_PIM_OP_MODE = 0x3ffd
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, true, data_temp_);
            }
            //Barrier();
            #ifdef debug_mode
            std::cout << "\nHOST:\tExecute μkernel\n";
            #endif
            // Execute ukernel 0-1
            for (int co_i = 0; co_i < 8; co_i++) {
                uint64_t co = co_o * 8 + co_i;
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    //channel, rank, bankgroup, bank, row, column
                    Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                }
            }

        }
    }
    
    // ExecuteBank(EVEN_BANK);
    // ExecuteBank(ODD_BANK); 

    // 추가적으로 global accumulator를 위한 연산이 추가 되어야 함

    Barrier();
}

void SpmvTransactionGenerator::GetResult() {
    //MULETEST에서 가져온 GetResult
    // Mode transition: AB -> SB
    #ifdef debug_mode
    std::cout << "HOST:\t[4] AB -> SB \n";
    #endif
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
        uint64_t hex_addr = ReverseAddressMapping(addr);
        TryAddTransaction(hex_addr, false, data_temp_);
    }
    Barrier();
    //여기까지는 모두 동일

    /*uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);
    // Read output data z
    #ifdef debug_mode
    std::cout << "\nHOST:\tRead output data z\n";
    #endif
    for (int offset = 0; offset < strided_size ; offset += SIZE_WORD)
        TryAddTransaction(addr_z_ + offset, false, z_ + offset);
    Barrier();*/
}

void SpmvTransactionGenerator::CheckResult() {
    //MUL에서 가져온 CheckResult
    /*int err = 0;
    float h_err = 0.;
    uint8_t *answer = (uint8_t *) malloc(sizeof(uint16_t) * n_);

    // Calculate actual answer of GEMV
    for (int i=0; i< n_; i++) {
        half h_x(*reinterpret_cast<half*>(&((uint16_t*)x_)[i]));
        half h_y(*reinterpret_cast<half*>(&((uint16_t*)y_)[i]));
        half h_answer = h_x * h_y;
        ((uint16_t*)answer)[i] = *reinterpret_cast<uint16_t*>(&h_answer);
    }

    // Calculate error
    for (int i=0; i< n_; i++) {
        half h_answer(*reinterpret_cast<half*>(&((uint16_t*)answer)[i]));
        half h_z(*reinterpret_cast<half*>(&((uint16_t*)z_)[i]));
        h_err += fabs(h_answer - h_z);  // fabs stands for float absolute value
    }
    std::cout << "ERROR : " << h_err << std::endl;*/
}

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//////////////CCCCCCC/////////PPPPPPPPPPPPP/////////UUU///////////UUU////////
//////////CCCCCCCCCCCCCCC/////PPP/////////PPPP//////UUU///////////UUU////////
/////////CCC//////////CCC/////PPP//////////PPPP/////UUU///////////UUU////////
////////CCC///////////////////PPP///////////PPP/////UUU///////////UUU////////
////////CCC///////////////////PPP/////////PPPP//////UUU///////////UUU////////
////////CCC///////////////////PPPPPPPPPPPPP/////////UUU///////////UUU////////
////////CCC///////////////////PPP///////////////////UUU///////////UUU////////
/////////CCC/////////CCCCC////PPP////////////////////UUU/////////UUU/////////
//////////CCCCCCCCCCCCC///////PPP//////////////////////UUUU///UUUU///////////
/////////////CCCCCCC//////////PPP/////////////////////////UUUUU//////////////
///////////////////////////////////////////////////////////////////KKM//LHY//

//여기 작성해야 됨 -> CPU가 실행했을 때 memory clock cycle을 측정해야 하기에 필요
////////////////////////////TW Added///////////////////////////////////////////
// Initialize variables and ukernel
void CPUSpmvTransactionGenerator::Initialize() {
    // Compute the starting addresses for each array in memory
    addr_row_indices_ = 0;
    addr_col_indices_ = Ceiling(nnz_ * sizeof(uint32_t), SIZE_ROW * NUM_BANK);
    addr_val_ = addr_col_indices_ + Ceiling(nnz_ * sizeof(uint32_t), SIZE_ROW * NUM_BANK);
    addr_x_ = addr_val_ + Ceiling(nnz_ * sizeof(uint16_t), SIZE_ROW * NUM_BANK);
    addr_y_ = addr_x_ + Ceiling(n_cols_ * UNIT_SIZE, SIZE_ROW * NUM_BANK);

    // Clear caches
    cache_col_.clear();
    cache_x_.clear();
}

void CPUSpmvTransactionGenerator::Execute() {
    for (uint32_t idx = 0; idx < nnz_; idx++) {
        uint32_t row, col;
        uint16_t value;

        // Load row index (row_indices[idx])
        TryAddTransaction(addr_row_indices_ + idx * sizeof(uint32_t), false, data_temp_);

        // Check if column index is in cache
        uint64_t col_addr = addr_col_indices_ + idx * sizeof(uint32_t);
        if (cache_col_.find(col_addr) == cache_col_.end()) {
            std::cout << "CPU: Cache miss for column index" << std::endl;
            TryAddTransaction(col_addr, false, data_temp_);
            cache_col_[col_addr] = true; // Mark as cached
        }

        // Check if value is in cache
        uint64_t val_addr = addr_val_ + idx * sizeof(uint16_t);
        TryAddTransaction(val_addr, false, data_temp_); // Always read value (no cache for value)

        // Check if x[col] is in cache
        uint64_t x_addr = addr_x_ + col * UNIT_SIZE;
        if (cache_x_.find(x_addr / 32) == cache_x_.end()) {
            TryAddTransaction(x_addr, false, data_temp_);
            cache_x_[x_addr / 32] = true; // Cache x[col] by 32B block
        }

        Barrier(); // Ensure all reads are completed before the next transaction

        // Write to y[row] (simulate write transaction)
        uint64_t y_addr = addr_y_ + row * UNIT_SIZE;
        TryAddTransaction(y_addr, true, data_temp_);

        Barrier(); // Ensure proper transaction order
    }
}
////////////////////////////TW Added end///////////////////////////////////////

}  // namespace dramsim3
