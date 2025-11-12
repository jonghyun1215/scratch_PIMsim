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

void SpmmTransactionGenerator::Initialize() {
    std::cout<<"Initialize SpmmTransactionGenerator" << std::endl;
    addr_B0_ = 0; // base address
    addr_B2_ = 0;
    addr_output_matrix_ = 0;
    max_b0 = true;
    max_index = 0;
    // ukernel을 몇번 실행시킬지 결정하기 위해 추가한 코드
    // 가장 row를 많이 차지하는 DRAF_BG를 찾아서 그것을 기준으로 ukernel_count_per_pim_를 결정
    uint32_t max_exec_time = 0;
    uint32_t min_exec_time = B0_data_[0].size() * 2;

    // Iterate over bank data to find the maximum size
    for (size_t i = 0; i < B0_data_.size(); ++i) {
        uint32_t add_kernel_executime = 0;
        // for (size_t j = 0; j < B0_data_[i].size(); ++j) {
        //     // 각 요소의 n_chunk 값이 1보다 크면 추가 실행 시간 증가
        //     if (B0_data_[i][j].n_chunk > 1) {
        //         add_kernel_executime++;
        //     }
        // }
        // 현재 블록의 총 실행 시간 = 기본 크기 + 추가 실행 시간
        uint32_t current_total_time = B0_data_[i].size() + add_kernel_executime;
        
        // 최대값, 최소값 갱신
        if (current_total_time > max_exec_time) {
            max_exec_time = current_total_time;
            max_index = i;
        }        
        if (current_total_time < min_exec_time) {
            min_exec_time = current_total_time;
        }
        
    }
    for (size_t i = 0; i < B2_data_.size(); ++i) {
        uint32_t add_kernel_executime = 0;
        // for (size_t j = 0; j < B2_data_[i].size(); ++j) {
        //     if (B2_data_[i][j].n_chunk > 1) {
        //         add_kernel_executime++;
        //     }
        // }
        uint32_t current_total_time = B2_data_[i].size() + add_kernel_executime;

        if (current_total_time > max_exec_time) {
            max_exec_time = current_total_time;
            max_b0 = false;
            max_index = i;
        }
        if (current_total_time < min_exec_time) {
            min_exec_time = current_total_time;
        }
    }

    // 최종 결정된 최대 실행 시간을 kernel_execution_time_에 할당
    kernel_execution_time_ = max_exec_time;
    min_kernel_execution_time_ = min_exec_time;

    std::cout << "Max ukernel iteration: " << kernel_execution_time_ << std::endl;
    std::cout << "Min ukernel iteration: " << min_kernel_execution_time_ << std::endl;

    // SpMM Ukernel 정의 (총 11개 명령어)
    ukernel_spmm_ = (uint32_t *) malloc(sizeof(uint32_t) * 32); // 32개 명령어 분량 할당 (넉넉하게)
    
    ukernel_spmm_[0]=0b01000010000000001000000000000000;  // MOV(AAM0) GRF_A BANK
    ukernel_spmm_[1]=0b01001000010000001000000000000000;  // MOV(AAM0) SRF_M GRF_A           
    ukernel_spmm_[2]=0b10010101011000001000000000000000;  // MUL_DRF(AAM0) GRF_B DRF SRF_M   
    ukernel_spmm_[3]=0b10000100100100001000000000000000;  // ADD(AAM0) GRF_B GRF_B GRF_B
    ukernel_spmm_[4]=0b11010000010001000101100010100000;  // LOOP -3 GRF_A[2]    
    ukernel_spmm_[5]=0b11000100100000001000000000000000;  // SACC(AAM0) GRF_B GRF_B              
    ukernel_spmm_[6]=0b00010000000001000111100000011000;  // JUMP -7 16                       
    ukernel_spmm_[7]=0b01000000100000001000000000000000;  // MOV(AAM0) BANK GRF_B            
    ukernel_spmm_[8]=0b00010000000001000100100000001111;  // JUMP -1 7                       
    ukernel_spmm_[9]=0b00100000000000000000000000000000;  // EXIT

    // no BGA
    // ukernel_spmm_[0]=0b01000010000000001000000000000000;  // MOV(AAM0) GRF_A BANK
    // ukernel_spmm_[1]=0b01001000010000001000000000000000;  // MOV(AAM0) SRF_M GRF_A           
    // ukernel_spmm_[2]=0b10010101011000001000000000000000;  // MUL_DRF(AAM0) GRF_B DRF SRF_M   
    // ukernel_spmm_[3]=0b10000100100100001000000000000000;  // ADD(AAM0) GRF_B GRF_B GRF_B
    // ukernel_spmm_[4]=0b11010000010001000110000010100000;  // LOOP -4 GRF_A[2]                       
    // ukernel_spmm_[5]=0b00010000000001000111100000001000;  // JUMP -7 8                     
    // ukernel_spmm_[6]=0b01000000100000001000000000000000;  // MOV(AAM0) BANK GRF_B            
    // ukernel_spmm_[7]=0b00010000000001000100100000001111;  // JUMP -1 7                       
    // ukernel_spmm_[8]=0b00100000000000000000000000000000;  // EXIT

}
void SpmmTransactionGenerator::SetData() {
    // strided size of one operand with one computation part(minimum)
    // UNIT_SIZE = 2, SIZE_WORD = 32, NUM_BANK = 256
    // strided_size = 2 * 32 * 256 = 16384

    #ifdef debug_mode
    std::cout << "HOST:\tSet input data...\n";
    #endif
    
    // 어차피 GPU 메모리에 이미 올라와 있는 데이터들 굳이 계산 필요 없다.

    // // --- B0_data_를 Bank 0에 할당 ---
    // // 16(ch) * 4(bg) = 64개의 (채널, 뱅크그룹) 조합을 순회
    // for (size_t i = 0; i < B0_data_.size(); ++i) {
    //     uint32_t bg = i % 4;          // BankGroup (0, 1, 2, 3)
    //     uint32_t current_ch = i / 4;  // Channel (0 ~ 15)
        
    //     // 현재 (ch, bg)에 할당된 모든 블록(j)을 순회
    //     for (size_t j = 0; j < B0_data_[i].size(); ++j) {
    //         // 1. Bank(ba)를 0으로 고정합니다.
    //         uint32_t ba = 0;
    //         //    (각 블록이 bank 0의 새 row에 매핑됨)
    //         uint32_t ro = (uint32_t)j; 
    //         uint32_t co = 0;          // Column (기존 로직 유지)

    //         const sparse_row_format& element = B0_data_[i][j];
    //         exact_row_size row_data = {};
    //         for (int rd = 0; rd < element.n_row; rd++) {
    //             row_data.row_desc.push_back(element.row_desc[rd]);
    //         }
    //         for (int cc = 0; cc < element.n_chunk; cc++) {
    //             row_data.col_chunk.push_back(element.col_chunk[cc]);
    //         }
    //         const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&row_data);
            
    //         for (co = 0; co < 32; co++) { // 1KB 전부 다 씀
    //             // Address 생성 시 ba는 항상 0입니다.
    //             Address addr(current_ch, 0, bg, ba, ro, co); // (rank=0)
    //             uint64_t hex_addr = ReverseAddressMapping(addr); 
    //             TryAddTransaction(hex_addr, true, const_cast<uint8_t*>(data_ptr + co*SIZE_WORD));
    //         }
    //     }
    // }
    // for (size_t i = 0; i < B2_data_.size(); ++i) {
    //     uint32_t bg = i % 4;          // BankGroup (0, 1, 2, 3)
    //     uint32_t current_ch = i / 4;  // Channel (0 ~ 15)
        
    //     for (size_t j = 0; j < B2_data_[i].size(); ++j) {
    //         // 1. Bank(ba)를 2로 고정합니다.
    //         uint32_t ba = 2;
    //         uint32_t ro = (uint32_t)j; 
    //         uint32_t co = 0;

    //         const sparse_row_format& element = B2_data_[i][j];
    //         exact_row_size row_data = {};
    //         for (int rd = 0; rd < element.n_row; rd++) {
    //             row_data.row_desc.push_back(element.row_desc[rd]);
    //         }
    //         for (int cc = 0; cc < element.n_chunk; cc++) {
    //             row_data.col_chunk.push_back(element.col_chunk[cc]);
    //         }
    //         const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&row_data);

    //         for (co = 0; co < 32; co++) { // 1KB 전부 다 씀
    //             Address addr(current_ch, 0, bg, ba, ro, co); // (rank=0으로 가정)
    //             uint64_t hex_addr = ReverseAddressMapping(addr);
    //             TryAddTransaction(hex_addr, true, const_cast<uint8_t*>(data_ptr + co*SIZE_WORD));
    //         }
    //     }
    // }
    // Barrier();

    #ifdef debug_mode
    std::cout << "\nHOST:\tLoad Dense Matrix to DRF (512B)\n";
    #endif
    // DRF는 512B = 32B * 16 Rows. MAP_DRF(0x3ff7)의 col 0~15에 쓰기.
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        // for (int bg = 0; bg < 4; bg++) {
        for (int co = 0; co < 16; co++) {
            Address addr(ch, 0, 0, EVEN_BANK, MAP_DRF, co);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            // data_temp_에는 적절한 Dense Matrix 데이터가 들어있다고 가정
            TryAddTransaction(hex_addr, true, data_temp_);
        }
        // }
    }
    Barrier();

    //exit(1);
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

    #ifdef debug_mode
    std::cout << "\nHOST:\tProgram SpMM μkernel \n";
    #endif
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        // Ukernel이 총 10개 명령어로, 32Byte(명령어 8개) 단위로 전송 시 2번의 트랜잭션이 필요
        // co=0: 명령어 0~7 전송
        // co=1: 명령어 8~15 전송 (여기서는 8~9번 명령어와 나머지 NOP가 전송됨)
        for (int co = 0; co < 2; co++) {
            Address addr(ch, 0, 0, 0, MAP_CRF, co);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            // ukernel_spmm_ 배열에서 8개씩(32Byte) 끊어서 전송
            TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_spmm_[co * 8]);
        }
    }
    Barrier();
}
void SpmmTransactionGenerator::Execute() {
    #ifdef debug_mode
    std::cout << "HOST:\tkernel_execution_time: " << kernel_execution_time_ << std::endl;
    #endif
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
        total_n_rows[ch] = 0; // initialize counts
    }
    Barrier();
    #ifdef debug_mode
    std::cout << "\nHOST:\tExecute μkernel\n";
    #endif

    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
    // for (int ro = 0; ro < kernel_execution_time_; ro++) {
        int n_rows = (int) max_b0 ? B0_data_[ch * 4 + 0].size() : B2_data_[ch * 4 + 0].size(); // bankgroup 0 고정
        int accum_rds = 0;
        for (int ro = 0; ro < n_rows; ro++) {
        // for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            int rd_index = 0;
            int n_rds_per_row = (int) max_b0 ? B0_data_[ch * 4 + 0][ro].n_rd : B2_data_[ch * 4 + 0][ro].n_rd;
            total_n_rows[ch] += n_rds_per_row;
            // ukernel iteration per row buffer
            // int n_chunk_per_row_buffer = (int) max_b0 ? B0_data_[ch * 4 + 0][ro].n_chunk : B2_data_[ch * 4 + 0][ro].n_chunk;
            accum_rds = (n_rds_per_row > 16) ? accum_rds += (n_rds_per_row - 16) : accum_rds; // 16개 초과하는 RD가 있으면 누적
            int ad_iter = 1;
            if (accum_rds > 16) {
                ad_iter += 1;
                accum_rds = accum_rds - 16;
            }
            for (int ad = 0; ad < ad_iter; ad++ ) {
                for (int j = 0; j < 16; j++) { // JUMP 16번 반복 (j = 0 to 15)
                    // MOV(AAM0) GRF_A EVEN_BANK
                    int co = 0;
                    Address addr(ch, 0, 0, EVEN_BANK, ro, co); 
                    uint64_t hex_addr = ReverseAddressMapping(addr);
                    TryAddTransaction(addr_B0_ + hex_addr, false, data_temp_);

                    // Determine loop count
                    int loop = (int) max_b0 ? B0_data_[ch * 4 + 0][ro].row_count[rd_index] : B2_data_[ch * 4 + 0][ro].row_count[rd_index];
                    if (loop == 0 || ad > 0) loop = 1;
                    rd_index++;
                    for (int k = 0; k < loop; k++) { // loop
                        // Execute ukernel 0 MOV(AAM0) SRF_M GRF_A
                        // AAM(0) 및 col=0으로 트리거
                        Address addr_0(ch, 0, 0, EVEN_BANK, ro, co); 
                        uint64_t hex_addr_0 = ReverseAddressMapping(addr_0);
                        TryAddTransaction(addr_B0_ + hex_addr_0, false, data_temp_);
                        // ukernel[1]: MUL_DRF(AAM0) GRF_B DRF SRF_M
                        // GRF_A[24] -> DRF Index, SRF_M[0] * DRF[idx] -> GRF_B[j]
                        // AAM(0), col=1, dst=GRF_B[j] (j는 JUMP 루프 카운터)
                        Address addr_1(ch, 0, 0, EVEN_BANK, ro, j * 1); // col=j (AAM으로 GRF_B[j] 선택)
                        uint64_t hex_addr_1 = ReverseAddressMapping(addr_1);
                        TryAddTransaction(hex_addr_1, false, data_temp_);

                        // ukernel[2]: ADD(AAM0) GRF_B GRF_B GRF_B
                        Address addr_2(ch, 0, 0, EVEN_BANK, ro, j * 1); // col=j
                        uint64_t hex_addr_2 = ReverseAddressMapping(addr_2);
                        TryAddTransaction(hex_addr_2, false, data_temp_);

                        // ukernel[3]: LOOP -2 GRF_A[2] (PIM 유닛이 내부적으로 PPC를 1로 돌림)
                        Address addr_3(ch, 0, 0, EVEN_BANK, ro, j * 1); // col=j
                        uint64_t hex_addr_3 = ReverseAddressMapping(addr_3);
                        TryAddTransaction(hex_addr_3, false, data_temp_);
                    } // end loop
                    // ukernel[4]: SACC(AAM0) GRF_B GRF_B
                    Address addr_4(ch, 0, 0, EVEN_BANK, ro, j * 1); // col=j
                    TryAddTransaction(ReverseAddressMapping(addr_4), false, data_temp_);
                    
                    // ukernel[6]: JUMP -6 16 (PIM 유닛이 내부적으로 PPC를 1로 돌림)
                    Address addr_6(ch, 0, 0, EVEN_BANK, ro, j * 1); // col=j
                    TryAddTransaction(ReverseAddressMapping(addr_6), false, data_temp_);
                } // end jump
            } // end ad_iter
            // ukernel[8] (JUMP -1 7)에 의해 8번 반복 (w = 0 to 7)
            // GRF_B[0] ~ GRF_B[7]을 Bank 0, Col 0~7에 씀
            for (int w = 0; w < 8; w++) {
                // ukernel[7]: MOV(AAM0) BANK GRF_B
                // AAM(0), col=w, src0=GRF_B[w]
                Address addr_7(ch, 0, 0, ODD_BANK, ro, w); // col=w (0~7)
                TryAddTransaction(ReverseAddressMapping(addr_7), true, data_temp_); // Write=true

                // ukernel[8]: JUMP -1 7
                Address addr_8(ch, 0, 0, ODD_BANK, ro, w); // col=w (0~7)
                TryAddTransaction(ReverseAddressMapping(addr_8), true, data_temp_); // Write=true
            }

            // ukernel[9]: EXIT
            Address addr_9(ch, 0, 0, EVEN_BANK, ro, 8); // 다음 col (8)
            TryAddTransaction(ReverseAddressMapping(addr_9), false, data_temp_);

        }
        
    }

    Barrier();
}
void SpmmTransactionGenerator::GetResult() {
    // no need to change mode for write to bank
    // SB mode change 후, logic die로 partial data 전송
    #ifdef debug_mode
    std::cout << "HOST:\t[4] AB -> SB \n";
    #endif
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
        uint64_t hex_addr = ReverseAddressMapping(addr);
        TryAddTransaction(hex_addr, false, data_temp_);
    }
    Barrier();
    *data_temp_ |= 1;

    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        for (int ro = 0; ro < total_n_rows[ch]/32; ++ro) { // 1kb 당 32 x 16
            for (int co = 0; co < 32; co++) {
                Address addr(ch, 0, 0, ODD_BANK, ro, co); // ODD_BANK에 저장된 결과
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, false, data_temp_);
            }
        }
        for (int co = 0; co < 32; co++) {   
            Address addr(ch, 0, 0, ODD_BANK, 0, co); // 최종 write
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, true, data_temp_);
        }
    }

}
void SpmmTransactionGenerator::AdditionalAccumulation(){}

void SpmmTransactionGenerator::ChangeVector() {}

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

    // 가장 많은 row를 차지하는 DRAF_BG를 찾아서 그것을 기준으로 ukernel_count_per_pim_를 결정
    // 4를 나누는 것은 BG 기준으로 묶여 있기 때문에, 4등분이 이루어지는 것을 고려
    kernel_execution_time_ = DRAF_BG_[max_index].size() / 4; // ukernel_count_per_pim_
    std::cout << "Max # of rows: " << kernel_execution_time_ << std::endl;
    
    //Even bank
    // 하나의 ROW가 process 됨
    ukernel_spmv_[0]=0b01001000000000001000000000000000;  // MOV(AAM0) SRF_M BANK
    ukernel_spmv_[1]=0b10010010001000001000000000000000;  // MUL(AAM0) GRF_A BANK SRF_M
    ukernel_spmv_[2]=0b11000000000000001000000000000000;  // SACC(AAM0) BANK BANK
    ukernel_spmv_[3]=0b11000000000000001000000000000000;  // SACC(AAM0) BANK BANK
    ukernel_spmv_[4]=0b00010000000001000001100000000110;  // JUMP -3 6
    ukernel_spmv_[5]=0b01000000010000001000000000000000;  // MOV(AAM0) BANK GRF_A
    ukernel_spmv_[6]=0b00010000000001000000100000000110; //JUMP -1 6

    //Odd bank
    // 하나의 ROW가 process 됨
    ukernel_spmv_[7]=0b01001000000000001000000000000000;  // MOV(AAM0) SRF_M BANK
    ukernel_spmv_[8]=0b10010010001000001000000000000000;  // MUL(AAM0) GRF_A BANK SRF_M
    ukernel_spmv_[9]=0b11000000000000001000000000000000;  // SACC(AAM0) BANK BANK
    ukernel_spmv_[10]=0b11000000000000001000000000000000;  // SACC(AAM0) BANK BANK
    ukernel_spmv_[11]=0b00010000000001000001100000000110;  // JUMP -3 6
    ukernel_spmv_[12]=0b01000000010000001000000000000000;  // MOV(AAM0) BANK GRF_A
    ukernel_spmv_[13]=0b00010000000001000000100000000110;  //JUMP -1 6
    // Exit
    ukernel_spmv_[14]=0b00100000000000000000000000000000;  // EXIT

}

//Memory에 PIM연산을 위한 데이터를 저장하는 과정
//ROW 하나당 23번의 write 필요
//column index 1번, value 7번, row index 14번, vector 1번 = 23번
void SpmvTransactionGenerator::SetData(){
    // strided size of one operand with one computation part(minimum)
    // UNIT_SIZE = 2, SIZE_WORD = 32, NUM_BANK = 256
    // strided_size = 2 * 32 * 256 = 16384
    
    //uint64_t strided_size = Ceiling(n_ *x UNIT_SIZE, SIZE_WORD * NUM_BANK);

    #ifdef debug_mode
    std::cout << "HOST:\tSet input data...\n";
    #endif

    //std::cout<<"DRAF_BG_.size() : "<<DRAF_BG_.size()<<std::endl;
    for (size_t i = 0; i < DRAF_BG_.size(); ++i) {
        //std::cout<<"DRAF_BG_["<<i<<"].size() : "<<DRAF_BG_[i].size()<<std::endl;
        //i=0 ~ 63
        uint32_t bg = i % 4;          // Fixed bg for each DRAF_BG[i] //bg = 0, 1, 2, 3
        uint32_t ro = 0;     // Start row at 0 for each DRAF_BG[i]
        uint32_t current_ch = i / 4;  // Increment ch after every 4 bg //ch = 0 ~ 15

        for (size_t j = 0; j < DRAF_BG_[i].size(); ++j) {
            uint32_t ba = j % 4;      // Bank cycles 0, 1, 2, 3
            if (ba == 0 && j > 0) {
                ro++;        // Increment row after each full cycle of ba
            }

            uint32_t co = 0;          // Column (start of the row)

            // Construct the address
            //Address addr(current_ch, 0, bg, ba, ro, co);

            // Translate to physical address
            //uint64_t hex_addr = ReverseAddressMapping(addr);

            // Flatten re_aligned_dram_format into uint8_t*
            // 
            const re_aligned_dram_format& element = DRAF_BG_[i][j];
            const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&element);
            //size_t total_size = sizeof(re_aligned_dram_format);
            
            // TW added
            // To test for print the setting data
            bool taewoon_debugg = false; //데이터는 정상적으로 쓰여지고 있는 것을 확인
            int index_row = 0;
            int index_vec = 0;
            int index_val = 0;
            
            for (co = 0; co < 32; co++) {
                Address addr(current_ch, 0, bg, ba, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                if(co <= 21 || co ==29){
                    TryAddTransaction(hex_addr, true, const_cast<uint8_t*>(data_ptr + co*SIZE_WORD));
                    // (TODO) 여기 쓰이는 데이터가 맞는지 확인 필요
                    /*if(taewoon_debugg){
                        if(co == 0){ //Col = 0일 때는 32B 4B data 8개
                            for (uint32_t test=0; test < 7; test++) { //SIZE_WORD = 32
                                uint32_t value;
                                memcpy(&value, data_ptr + test*4, sizeof(uint32_t));
                                std::cout << "col index[" << test << "] = " <<  value << std::endl;
                            }
                        }
                        else if(co >= 1 && co <= 7){
                            for (uint32_t test=0; test < 16; test++) { //WORD_SIZE = 32
                                uint16_t value;
                                memcpy(&value, data_ptr + co * WORD_SIZE + test * 2, sizeof(uint16_t));
                                std::cout << "Value[" << index_val << "] = " <<  value << std::endl;
                                index_val++;
                            }
                        }
                        else if(co >= 8 && co <= 21){
                            //row index는 4Byte -> test = 0 ~ 7
                            for (uint32_t test=0; test < 8; test++) { //WORD_SIZE = 32
                                uint32_t value;
                                memcpy(&value, data_ptr + co * WORD_SIZE + test * 4, sizeof(uint32_t));
                                std::cout << "Row index[" << index_row << "] = " <<  value << std::endl;
                                index_row++;
                            }
                        }
                        else if(co == 29){
                            for (uint32_t test=0; test < 7; test++) { //WORD_SIZE = 32
                                uint16_t value;
                                memcpy(&value, data_ptr + co * WORD_SIZE + test * 2, sizeof(uint16_t));
                                std::cout << "Vector[" << index_vec << "] = " <<  value << std::endl;
                                
                                //추가적으로 MUL 연산할 때 값이 다르게 나오는 문제가 있어 해결 중
                                uint32_t test_col;
                                memcpy(&test_col, data_ptr + test*4, sizeof(uint32_t));
                                if(value ==0 && test_col != 0){
                                    std::cout <<" Only column is zero but value is not zero" << std::endl;
                                }
                                else if(value !=0 && test_col == 0){
                                    std::cout <<" Only value is zero but column is not zero" << std::endl;
                                }
                                if(test == 0 && value != 0 && test_col == 0){
                                    std::cout << "First value is not zero" << std::endl;
                                }
                                index_vec++;
                            }
                        }
                    }*/
                }
            }
        }
    }
    //exit(1);
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
        for (int co = 0; co < 3; co++) { //for (int co = 0; co < 1; co++) {
            Address addr(ch, 0, 0, 0, MAP_CRF, co);
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(hex_addr, true, (uint8_t*)&ukernel_spmv_[co*8]);
        }
    }
    Barrier();
}

void SpmvTransactionGenerator::Execute() {
    // NUM_WORD_PER_ROW = 32
    #ifdef debug_mode
    std::cout << "HOST:\tkernel_execution_time: " << kernel_execution_time_ << std::endl;
    #endif

    for (int ro = 0; ro < kernel_execution_time_; ro++) {
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
        // TW added
        // Barrier가 있어야 될거 같아서 추가
        Barrier();
        
        #ifdef debug_mode
        std::cout << "\nHOST:\tExecute μkernel\n";
        #endif

        #ifdef debug_mode
        std::cout << "\nHOST:\tExecute Evenbank\n";
        #endif

        // Execute ukernel 0 (MOV 명령어)
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            uint64_t co = 29;
            Address addr(ch, 0, 0, EVEN_BANK, ro, co); //Column 29 indicate vector
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
        }

        // Execute ukernel 1-4 (MUL, SACC, SACC, JUMP 명령어)
        // (TODO) 다음과 같이 동작하도록 구성해야 됨
        uint64_t sacc_offset = 7;
        for (uint64_t co = 0; co < 16; co++) { // JH modify
        // for (uint64_t co = 1; co < 8; co++) {
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                //channel, rank, bankgroup, bank, row, column
                Address addr(ch, 0, 0, EVEN_BANK, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                // 1. Transaction for trigger MUL
                TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                Address addr1(ch, 0, 0, EVEN_BANK, ro, co + sacc_offset); //8, 10...
                hex_addr = ReverseAddressMapping(addr1);
                // 2. Transaction for trigger SACC + NOP
                //SACC
                TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                //NOP
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                Address addr2(ch, 0, 0, EVEN_BANK, ro, co + sacc_offset+1); //9, 11...
                hex_addr = ReverseAddressMapping(addr2);
                // 3. Transaction for trigger SACC + NOP 
                //SACC
                TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                //NOP
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                // 4. JUMP는 자동으로
                sacc_offset++;
            }
        }
        // (TODO) MOV(AAM0) BANK GRF_A를 추가해야 됨
        // JUMP 로 MOV가 7번 수행 될 수 있게 JUMP -1 6로 설정
        // column = 22 ~ 28
        // for(uint64_t co = 22; co < 29; co++){
        for(uint64_t co = 0; co < 16; co++){ // JH modify
            for(int ch = 0; ch < NUM_CHANNEL; ch++){
                Address addr(ch, 0, 0, EVEN_BANK, false, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, false, data_temp_);
            }
        }
        
        // To trigger global accumulator
        /*for (uint64_t co = 22; co < 29; co++) {
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                //channel, rank, bankgroup, bank, row, column
                Address addr(ch, 0, 0, EVEN_BANK, TRIGGER_GACC, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                TryAddTransaction(hex_addr, false, data_temp_);
            }
        }*/

        #ifdef debug_mode
        std::cout << "\nHOST:\tExecute Oddbank\n";
        #endif
        
        // Execute ukernel 0 (MOV 명령어)
        #ifdef debug_mode
        std::cout << "\nHOST:\tExecute μkernel 0\n";
        #endif
        for (int ch = 0; ch < NUM_CHANNEL; ch++) {
            uint64_t co = 29;
            Address addr(ch, 0, 0, ODD_BANK, ro, co); //Column 29 indicate vector
            uint64_t hex_addr = ReverseAddressMapping(addr);
            TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
        }

        #ifdef debug_mode
        std::cout << "\nHOST:\tExecute μkernel 1-4\n";
        #endif

        // Execute ukernel 1-4 (MUL, SACC, SACC, JUMP 명령어)
        // (TODO) 다음과 같이 동작하도록 구성해야 됨
        sacc_offset = 7;
        // for (uint64_t co = 1; co < 8; co++) { 
        for (uint64_t co = 0; co < 16; co++) { // JH modify
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                //channel, rank, bankgroup, bank, row, column
                Address addr(ch, 0, 0, ODD_BANK, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                // 1. Transaction for trigger MUL
                TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                Address addr1(ch, 0, 0, ODD_BANK, ro, co + sacc_offset);
                hex_addr = ReverseAddressMapping(addr1);
                // 2. Transaction for trigger SACC + NOP
                //SACC
                TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                //NOP
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                Address addr2(ch, 0, 0, ODD_BANK, ro, co + sacc_offset+1);
                hex_addr = ReverseAddressMapping(addr2);
                // 3. Transaction for trigger SACC + NOP
                //SACC
                TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                //NOP
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                // 4. JUMP는 자동으로
                sacc_offset++;
            }
        }

        // (TODO) MOV(AAM0) BANK GRF_A를 추가해야 됨
        // JUMP 로 MOV가 7번 수행 될 수 있게 JUMP -1 6로 설정
        // column = 22 ~ 28
        // for(uint64_t co = 22; co < 29; co++){
        for(uint64_t co = 0; co < 16; co++){ // JH modify
            for(int ch = 0; ch < NUM_CHANNEL; ch++){
                Address addr(ch, 0, 0, ODD_BANK, false, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(hex_addr, false, data_temp_);
            }
        }

        /*
        // Global accumulator trigger 하기 위한 코드
        // Shared accumulator 동작 검증 후 주석 풀고
        // 동작 검증 필요 
        #ifdef debug_mode
        std::cout << "\nHOST:\tExecute Global Accumulator\n";
        #endif
        
        // To trigger global accumulator
        for (uint64_t co = 22; co < 29; co++) {
            for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                //channel, rank, bankgroup, bank, row, column
                Address addr(ch, 0, 0, ODD_BANK, TRIGGER_GACC, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
                TryAddTransaction(hex_addr, false, data_temp_);
            }
        }*/
        
    }

    Barrier();
}

void SpmvTransactionGenerator::GetResult() {
    //MULETEST에서 가져온 GetResult
    // Mode transition: AB -> SB
    // Try add transaction에 값을 넣을 때
    
    //*data_temp_ |= 1; -> 이걸 안해주면 에러가 발생함 (값이 없으면 에러가 발생하는 듯)

    #ifdef debug_mode
    std::cout << "HOST:\t[4] AB -> SB \n";
    #endif
    for (int ch = 0; ch < NUM_CHANNEL; ch++) {
        Address addr(ch, 0, 0, 0, MAP_SBMR, 0);
        uint64_t hex_addr = ReverseAddressMapping(addr);
        TryAddTransaction(hex_addr, false, data_temp_);
    }
    Barrier();
    uint8_t *data_temp_ = (uint8_t *) malloc(burstSize_);
    uint8_t *index_temp_ = (uint8_t *) malloc(burstSize_);
    uint8_t *partial_index_ = (uint8_t *) malloc(burstSize_);
    uint8_t *partial_value_ = (uint8_t *) malloc(burstSize_);
    #ifdef debug_mode
    std::cout << "\nHOST:\tRead output data\n";
    #endif

    // 1044869번의 memory cycle, 22972의 Loop count
    for (size_t i = 0; i < DRAF_BG_.size(); ++i) {
        uint32_t bg = i % 4;          // Fixed bg for each DRAF_BG[i] //bg = 0, 1, 2, 3
        uint32_t ro = 0;     // Start row at 0 for each DRAF_BG[i]
        uint32_t ch = i / 4;  // Increment ch after every 4 bg //ch = 0 ~ 15

        for (size_t j = 0; j < DRAF_BG_[i].size(); ++j) {
            uint32_t ba = j % 4;      // Bank cycles 0, 1, 2, 3
            if (ba == 0 && j > 0) {
                ro++;        // Increment row after each full cycle of ba
            }
            // 위에 ro, bg, bg, ba, ch 까지 정의
            // for (uint64_t co = 8; co <= 21; co++) {
            //     Address addr(ch, 0, bg, ba, ro, co);
            //     uint64_t hex_addr = ReverseAddressMapping(addr);
            //     TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_index_);
            //     //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
            // }
            // for (uint64_t co = 22; co < 29; co++) {
            //     Address addr(ch, 0, bg, ba, ro, co);
            //     uint64_t hex_addr = ReverseAddressMapping(addr);
            //     TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_value_);
            //     //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
            // }
            // JH modify
            for (uint64_t co = 0; co < 16; co++) {
                Address addr(ch, 0, bg, ba, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_value_);
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
            }
        }
    }

    // 483920의 memory cycle, 23040의 Loop count
    //BG accumulator region의 데이터를 읽어와 추가적인 accumulating을 하는 경우를 가정
    //읽어올 때, 데이터가 0 일경우 Row index도 0으로 처리하여, 연산에서 빠지도록 했음을 가정
 
    // 데이터를 읽어오는 과정
    /*for (int ro = 0; ro < kernel_execution_time_; ro++) {
        for(int BA = 0; BA < 4; BA++){
            for(int BG = 0; BG < 4; BG++){
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    for (uint64_t co = 8; co <= 21; co++) {
                        Address addr(ch, 0, BG, BA, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_index_);
                    }
                    for (uint64_t co = 22; co < 29; co++) {
                        Address addr(ch, 0, BG, BA, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_value_);
                    }
                }
            }
        }
    }*/
    Barrier();

    // To print accumulation count
    memory_system_.PrintAccumulateCount();
}

void SpmvTransactionGenerator::AdditionalAccumulation(){
    #ifdef debug_mode
    std::cout << "HOST:\t Additional accumulation start \n";
    #endif
    //위에서 읽어온 데이터를 accumulate 하는데 걸리는 cycle에 대한 측정
}

//Check result가 없으면 자동으로 없을 때를 기준으로 0값을 반환
/*void SpmvTransactionGenerator::CheckResult() {
    //MUL에서 가져온 CheckResult
    int err = 0;
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
    std::cout << "ERROR : " << h_err << std::endl;
}*/

void SpmvTransactionGenerator::ChangeVector() {
    
}

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
/////////// TO measure cylce when no PIM //////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
void NoPIMSpmvTransactionGenerator::Initialize() { //여기는 코딩 끝
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

    // 가장 많은 row를 차지하는 DRAF_BG를 찾아서 그것을 기준으로 ukernel_count_per_pim_를 결정
    // 4를 나누는 것은 BG 기준으로 묶여 있기 때문에, 4등분이 이루어지는 것을 고려
    kernel_execution_time_ = DRAF_BG_[max_index].size() / 4; // ukernel_count_per_pim_
    std::cout << "Max # of rows: " << kernel_execution_time_ << std::endl;

}

//Memory에 PIM연산을 위한 데이터를 저장하는 과정
//ROW 하나당 23번의 write 필요
//column index 1번, value 7번, row index 14번, vector 1번 = 23번
void NoPIMSpmvTransactionGenerator::SetData(){
    // strided size of one operand with one computation part(minimum)
    // UNIT_SIZE = 2, SIZE_WORD = 32, NUM_BANK = 256
    // strided_size = 2 * 32 * 256 = 16384
    
    //uint64_t strided_size = Ceiling(n_ * UNIT_SIZE, SIZE_WORD * NUM_BANK);

    #ifdef debug_mode
    std::cout << "HOST:\tSet input data...\n";
    #endif

    //std::cout<<"DRAF_BG_.size() : "<<DRAF_BG_.size()<<std::endl;
    for (size_t i = 0; i < DRAF_BG_.size(); ++i) {
        //std::cout<<"DRAF_BG_["<<i<<"].size() : "<<DRAF_BG_[i].size()<<std::endl;
        //i=0 ~ 63
        uint32_t bg = i % 4;          // Fixed bg for each DRAF_BG[i] //bg = 0, 1, 2, 3
        uint32_t ro = 0;     // Start row at 0 for each DRAF_BG[i]
        uint32_t current_ch = i / 4;  // Increment ch after every 4 bg //ch = 0 ~ 15

        for (size_t j = 0; j < DRAF_BG_[i].size(); ++j) {
            uint32_t ba = j % 4;      // Bank cycles 0, 1, 2, 3
            if (ba == 0 && j > 0) {
                ro++;        // Increment row after each full cycle of ba
            }

            uint32_t co = 0;          // Column (start of the row)

            // Construct the address
            //Address addr(current_ch, 0, bg, ba, ro, co);

            // Translate to physical address
            //uint64_t hex_addr = ReverseAddressMapping(addr);

            // Flatten re_aligned_dram_format into uint8_t*
            // 
            const re_aligned_dram_format& element = DRAF_BG_[i][j];
            const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(&element);
            //size_t total_size = sizeof(re_aligned_dram_format);
            
            // TW added
            // To test for print the setting data
            bool taewoon_debugg = false; //데이터는 정상적으로 쓰여지고 있는 것을 확인
            int index_row = 0;
            int index_vec = 0;
            int index_val = 0;
            
            for (co = 0; co < 32; co++) {
                Address addr(current_ch, 0, bg, ba, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                if(co <= 21 || co ==29){
                    TryAddTransaction(hex_addr, true, const_cast<uint8_t*>(data_ptr + co*SIZE_WORD));
                }
            }
        }
    }
    Barrier();
}

void NoPIMSpmvTransactionGenerator::GetResult() {
    //MULETEST에서 가져온 GetResult
    // Mode transition: AB -> SB
    // Try add transaction에 값을 넣을 때
    
    //*data_temp_ |= 1; -> 이걸 안해주면 에러가 발생함 (값이 없으면 에러가 발생하는 듯)

    uint8_t *data_temp_ = (uint8_t *) malloc(burstSize_);
    uint8_t *index_temp_ = (uint8_t *) malloc(burstSize_);
    uint8_t *partial_index_ = (uint8_t *) malloc(burstSize_);
    uint8_t *partial_value_ = (uint8_t *) malloc(burstSize_);
    #ifdef debug_mode
    std::cout << "\nHOST:\tRead output data\n";
    #endif

    // 1044869번의 memory cycle, 22972의 Loop count
    for (size_t i = 0; i < DRAF_BG_.size(); ++i) {
        uint32_t bg = i % 4;          // Fixed bg for each DRAF_BG[i] //bg = 0, 1, 2, 3
        uint32_t ro = 0;     // Start row at 0 for each DRAF_BG[i]
        uint32_t ch = i / 4;  // Increment ch after every 4 bg //ch = 0 ~ 15

        for (size_t j = 0; j < DRAF_BG_[i].size(); ++j) {
            uint32_t ba = j % 4;      // Bank cycles 0, 1, 2, 3
            if (ba == 0 && j > 0) {
                ro++;        // Increment row after each full cycle of ba
            }
            // 위에 ro, bg, bg, ba, ch 까지 정의
            for (uint64_t co = 8; co <= 21; co++) {
                Address addr(ch, 0, bg, ba, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_index_);
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
            }
            for (uint64_t co = 22; co < 29; co++) {
                Address addr(ch, 0, bg, ba, ro, co);
                uint64_t hex_addr = ReverseAddressMapping(addr);
                TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_value_);
                //TryAddTransaction(addr_DRAF_ + hex_addr, false, data_temp_);
            }
        }
    }

    // 483920의 memory cycle, 23040의 Loop count
    //BG accumulator region의 데이터를 읽어와 추가적인 accumulating을 하는 경우를 가정
    //읽어올 때, 데이터가 0 일경우 Row index도 0으로 처리하여, 연산에서 빠지도록 했음을 가정

    // 데이터를 읽어오는 과정
    /*for (int ro = 0; ro < kernel_execution_time_; ro++) {
        for(int BA = 0; BA < 4; BA++){
            for(int BG = 0; BG < 4; BG++){
                for (int ch = 0; ch < NUM_CHANNEL; ch++) {
                    for (uint64_t co = 8; co <= 21; co++) {
                        Address addr(ch, 0, BG, BA, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_index_);
                    }
                    for (uint64_t co = 22; co < 29; co++) {
                        Address addr(ch, 0, BG, BA, ro, co);
                        uint64_t hex_addr = ReverseAddressMapping(addr);
                        TryAddTransaction(addr_DRAF_ + hex_addr, false, partial_value_);
                    }
                }
            }
        }
    }*/
    Barrier();
}

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
/////////////////////////////////////////////////////////////////////////////

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
    
    //Changing column only
    //Address addr(current_ch, 0, bg, ba, ro, co);
    for(uint32_t co = 0; co< 32; co++){
        Address addr(0, 0, 0, 0, 0, co);
        uint64_t hex_addr = ReverseAddressMapping(addr);
        TryAddTransaction(hex_addr, true, data_temp_);
    }

    Barrier();
    
    for (uint32_t idx = 0; idx < 1; idx++) {
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
