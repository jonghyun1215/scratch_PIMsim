#include "./pim_func_sim.h"
#include <assert.h>
#include <iostream>

namespace dramsim3 {

PimFuncSim::PimFuncSim(Config &config)
    : config_(config) {
    // Set pim_unit's id by its order of pim_unit (= pim_index)
    //하기에 2개 Bank 당 
    // 수정 필요
    for (int i=0; i< config_.channels * config_.banks / 2; i++) {
        pim_unit_.push_back(new PimUnit(config_, i));
    }
    //TW added to initialize shared_acc_ and global_acc_
    for (int i=0; i< config_.channels * config_.banks / 4; i++) {
        PimUnit& pim1 = *pim_unit_[i * 2];      // PimUnit을 참조
        PimUnit& pim2 = *pim_unit_[i * 2 + 1];  // 
        shared_acc_.push_back(new SharedAccumulator(config_, i, pim1, pim2));
    }
    //우선 global accumulator는 하나만 선언
    //channel에서 input을 받아서 넘겨주는 방식
    //vecotr형으로 선언 되어 있기 때문에, 여러개의 global accumulator를 선언할 수 있음
    // i도 넣어서, id를 표시해 줘야 됨
    global_acc_.push_back(new GlobalAccumulator(config_));

    accumulation_count = 0;
}

void PimFuncSim::init(uint8_t* pmemAddr_, uint64_t pmemAddr_size_,
                      unsigned int burstSize_) {
    burstSize = burstSize_;
    pmemAddr_size = pmemAddr_size_;
    pmemAddr = pmemAddr_;

    // Set default bankmode of channel to "SB"
    // _config.channels = 16
    for (int i=0; i< config_.channels; i++) {
        bankmode.push_back("SB");
        PIM_OP_MODE.push_back(false);
    }
    std::cout << "PIM_OP_MODE initialized with" \
            << config_.channels << " channels\n";
    std::cout << "PimFuncSim initialized!\n";

    for (int i=0; i< config_.channels * config_.banks / 2; i++) {
        pim_unit_[i]->init(pmemAddr, pmemAddr_size, burstSize);
        if(i%2 == 0) {
            shared_acc_[i/2]->init(pmemAddr, pmemAddr_size, burstSize);
        }
    }
    global_acc_[0]->init(pmemAddr, pmemAddr_size, burstSize); 
    std::cout << "pim_units initialized!\n";
}

// Map structured address into 64-bit hex_address
uint64_t PimFuncSim::ReverseAddressMapping(Address& addr) {
    uint64_t hex_addr = 0;
    hex_addr += (uint64_t)addr.channel << config_.ch_pos;
    hex_addr += (uint64_t)addr.rank << config_.ra_pos;
    hex_addr += (uint64_t)addr.bankgroup << config_.bg_pos;
    hex_addr += (uint64_t)addr.bank << config_.ba_pos;
    hex_addr += (uint64_t)addr.row << config_.ro_pos;
    hex_addr += (uint64_t)addr.column << config_.co_pos;
    return hex_addr << config_.shift_bits;
}

// Return pim_index of pim_unit that input address accesses
uint64_t PimFuncSim::GetPimIndex(Address& addr) {
    return (addr.channel * config_.banks +
            addr.bankgroup * config_.banks_per_group +
            addr.bank) / 2;
}

// Return to print out debugging information or not
//  Can set debug_mode and watch_pimindex at pim_config.h
bool PimFuncSim::DebugMode(uint64_t hex_addr) {
    #ifdef debug_mode
    Address addr = config_.AddressMapping(hex_addr);
    int pim_index = GetPimIndex(addr);
    if (pim_index == watch_pimindex / (config_.banks / 2)) return true;

    #endif
    return false;
}

// Change bankmode when transaction with certain row address is recieved
//SB mode, AB mode, PIM mode 총 3가지 mode 존재
//얘는 그냥 사용해도 됨
bool PimFuncSim::ModeChanger(uint64_t hex_addr) {
    Address addr = config_.AddressMapping(hex_addr);
    if (addr.row == 0x3fff) { // MAP_SBMR = 0x3fff
        if (bankmode[addr.channel] == "AB") {
            bankmode[addr.channel] = "SB";
            // TW added
            //강제적으로 맞추기 위해 추가
            // PIM_OP_MODE[addr.channel] = false;
        }
        if (DebugMode(hex_addr))
            std::cout << " Pim_func_sim: AB → SB mode change\n";
        return true;
    } else if (addr.row == 0x3ffe) { //MAP_ABMR = 0x3ffe
        if (bankmode[addr.channel] == "SB") {
            bankmode[addr.channel] = "AB";
        }
        if (DebugMode(hex_addr))
            std::cout << " Pim_func_sim: SB → AB mode change\n";
        return true;
    } else if (addr.row == 0x3ffd) { //MAP_PIM_OP_MODE = 0x3ffd
        PIM_OP_MODE[addr.channel] = true;
        if (DebugMode(hex_addr))
            std::cout << " Pim_func_sim: AB → PIM mode change\n";
        return true;
    }
    return false;
}

// Write DataPtr data to physical memory address of hex_addr
// 기존 DRAMsim3는 데이터를 저장할 수 없어서 PIMFuncSim에서 데이터를 저장하도록 수정
void PimFuncSim::PmemWrite(uint64_t hex_addr, uint8_t* DataPtr) {
    uint8_t *host_addr = pmemAddr + hex_addr;
    memcpy(host_addr, DataPtr, burstSize);
}

// Read data from physical memory address of hex_addr to DataPtr
// 기존 DRAMsim3는 데이터를 저장할 수 없어서 PIMFuncSim에서 데이터를 저장하도록 수정
void PimFuncSim::PmemRead(uint64_t hex_addr, uint8_t* DataPtr) {
    uint8_t *host_addr = pmemAddr + hex_addr;
    memcpy(DataPtr, host_addr, burstSize);
}

//  Performs physical memory RD/WR, bank mode change, set PIM register,
//  execute PIM computation and write result to physical memory
//  AddTransaction을 통해 transaction을 받아서 처리하는 함수
//  dram_system.cc에서 memory와 pim_func_sim 두개에 transaction을 같이 보냄
//  Transaction에는 addr, is_write, DataPtr이 들어있음
//  dram_system.cc에서 정보가 넘어옴

void PimFuncSim::AddTransaction(Transaction *trans) {
    uint64_t hex_addr = (*trans).addr;
    bool is_write = (*trans).is_write;
    uint8_t* DataPtr = (*trans).DataPtr;
    //To print size of total DataPtr
    Address addr = config_.AddressMapping(hex_addr);
    (*trans).executed_bankmode = bankmode[addr.channel];

    // Change bankmode register if transaction has certain row address
    bool is_mode_change = ModeChanger(hex_addr);
    if (is_mode_change)
        return;

    if (PIM_OP_MODE[addr.channel] == false) { //PIM mode가 아닌 경우
        if (bankmode[addr.channel] == "SB") {
            // Execute transaction on SB(Single Bank) mode
            (*trans).executed_bankmode = "SB";
            if (DebugMode(hex_addr))
                std::cout << " Pim_func_sim: SB mode → ";

            // Address가 각각 channel, rank, bankgroup, bank, row, column으로 나눠져 있음
            // Set PIM registers or RD/WR to Physical memory
            //  Discerned with certain row address
            if (addr.row == 0x3ffa) {  // set SRF_A, SRF_M
                if (DebugMode(hex_addr))
                    std::cout << "SetSrf\n";
                int pim_index = GetPimIndex(addr);
                pim_unit_[pim_index]->SetSrf(hex_addr, DataPtr);

            } else if (addr.row == 0x3ffb) {  // set GRF_A, GRF_B
                if (DebugMode(hex_addr))
                    std::cout << "SetGrf\n";
                int pim_index = GetPimIndex(addr);
                pim_unit_[pim_index]->SetGrf(hex_addr, DataPtr);

            } else if (addr.row == 0x3ffc) {  // set CRF
                if (DebugMode(hex_addr))
                    std::cout << "SetCrf\n";
                int pim_index = GetPimIndex(addr);
                pim_unit_[pim_index]->SetCrf(hex_addr, DataPtr);

            } else {  // RD, WR
                if (DebugMode(hex_addr))
                    std::cout << "RD/WR\n";
                if (is_write) {
                    PmemWrite(hex_addr, DataPtr);
                } else {
                    PmemRead(hex_addr, DataPtr);
                }
            }

        } else if (bankmode[addr.channel] == "AB") {
            // Execute transaction on AB(All Bank) mode
            (*trans).executed_bankmode = "AB";
            if (!PIM_OP_MODE[addr.channel]) {
                if (DebugMode(hex_addr))
                    std::cout << " Pim_func_sim: AB mode → ";

                // Set (PIM registers or RD/WR to Physical memory) of all
                // banks in a channel
                //  Discerned with certain row address
                // TW added 내 경우에서는 set SRF_A와 SRF_M을 할 필요가 없을 듯

                if (addr.row == 0x3ffa) {  // set SRF_A, SRF_M
                    if (DebugMode(hex_addr))
                        std::cout << "SetSrf\n";
                    for (int i=0; i< config_.banks/2; i++) {
                        int pim_index = GetPimIndex(addr) + i;
                        pim_unit_[pim_index]->SetSrf(hex_addr, DataPtr);
                    }

                } else if (addr.row == 0x3ffb) {  // set GRF_A, GRF_B
                    if (DebugMode(hex_addr))
                        std::cout << "SetGrf\n";
                    for (int i=0; i< config_.banks/2; i++) {
                        int pim_index = GetPimIndex(addr) + i;
                        pim_unit_[pim_index]->SetGrf(hex_addr, DataPtr);
                    }
                // 0x3ffc = 0b11111111111100
                } else if (addr.row == 0x3ffc) {  // set CRF
                    if (DebugMode(hex_addr))
                        std::cout << "SetCrf\n";
                    for (int i=0; i< config_.banks/2; i++) {
                        int pim_index = GetPimIndex(addr) + i;
                        pim_unit_[pim_index]->SetCrf(hex_addr, DataPtr);
                    }
                }
                //TW added
                // 0x3ff9 = TRIGGER_GACC
                else if (addr.row == 0x3ff9) {
                    if (DebugMode(hex_addr))
                    {
                        std::cout << "channel : " << addr.channel;
                        std::cout << " Bank : " << addr.bank;
                        std::cout << " Triggering global accumulator\n";
                    }
                    //TW added
                    std::cout << "Pim_func_sim: Triggering global accumulator\n";
                    global_acc_[0]->StartAcc();
                    // 추후 global_acc_[1]도 추가할 수 있음
                    // global_acc_[1]->StartAcc();
                }          

                else {  // RD, WR
                    // check if it is evenbank or oddbank
                    int evenodd = addr.bank % 2;
                    if (DebugMode(hex_addr))
                        std::cout << "RD/WR\n";
                    for (int i=evenodd; i< config_.banks; i+=2) {
                        Address tmp_addr = Address(addr.channel, addr.rank, i/4,
                                                   i%4, addr.row, addr.column);
                        uint64_t tmp_hex_addr = ReverseAddressMapping(tmp_addr);

                        if (is_write)
                            PmemWrite(tmp_hex_addr, DataPtr);
                        else
                            PmemRead(tmp_hex_addr, DataPtr);
                    }
                }
            }
        }
    } else { //PIM mode인 경우 = PIM_OP_MODE[addr.channel] == true
        // Execute transaction on AB-PIM(All Bank PIM) mode
        (*trans).executed_bankmode = "PIM";
        if (DebugMode(hex_addr))
            std::cout << " Pim_func_sim: PIM mode → ";

        // Same as AB mode except, sends Transaction to proper pim_unit
        // when RD/WR transaction is recieved
        //  Discerned with certain row address
        if (addr.row == 0x3ffa) {  // set SRF_A, SRF_M
            if (DebugMode(hex_addr))
                std::cout << "SetSrf\n";
            for (int i=0; i< config_.banks/2; i++) {
                int pim_index = GetPimIndex(addr) + i;
                pim_unit_[pim_index]->SetSrf(hex_addr, DataPtr);
            }
        } else if (addr.row == 0x3ffb) {  // set GRF_A, GRF_B
            if (DebugMode(hex_addr))
                std::cout << "SetGrf\n";
            for (int i=0; i< config_.banks/2; i++) {
                int pim_index = GetPimIndex(addr) + i;
                pim_unit_[pim_index]->SetGrf(hex_addr, DataPtr);
            }
        } else if (addr.row == 0x3ffc) {  // set CRF
            if (DebugMode(hex_addr))
                std::cout << "SetCrf\n";
            for (int i=0; i< config_.banks/2; i++) {
                int pim_index = GetPimIndex(addr) + i;
                pim_unit_[pim_index]->SetCrf(hex_addr, DataPtr);
            }
        } else if (addr.row == 0x3ff9) {
            if (DebugMode(hex_addr))
            {
                std::cout << "channel : " << addr.channel;
                std::cout << " Bank : " << addr.bank;
                std::cout << " Triggering global accumulator\n";
            }
            // TW added
            global_acc_[0]->StartAcc();
            // 추후 global_acc_[1]도 추가할 수 있음
            // global_acc_[1]->StartAcc();
        } else if (addr.row == 0x3ff7) { // JH added set DRF
            if (DebugMode(hex_addr))
                std::cout << "SetDrf\n";
            for (int i=0; i< config_.banks/2; i++) {
                int pim_index = GetPimIndex(addr) + i;
                pim_unit_[pim_index]->SetDrf(hex_addr, DataPtr);
            }
        }     
         else {  // RD, WR
            // check if it is evenbank or oddbank
            int evenodd = addr.bank % 2;
            if (DebugMode(hex_addr))
                std::cout << "RD/WR (Trigger PIM inst.)\n";
            for (int i=evenodd; i< config_.banks; i+=2) {
                Address tmp_addr = Address(addr.channel, addr.rank, i/4,
                                           i%4, addr.row, addr.column);
                uint64_t tmp_hex_addr = ReverseAddressMapping(tmp_addr);

                int pim_index = GetPimIndex(addr) + i/2;

                //shared_acc에 물려있는 pim_unit에 access 할 수 있도록 수정
                //trnasaction_generator.cc 에서 하나의 transaction을 보내도,
                //여기서 even / odd 전체 bank에 대해서 transaction을 보냄
                int ret = shared_acc_[pim_index/2]->pim_unit_[pim_index%2]->AddTransaction(tmp_hex_addr,
                                                               is_write,
                                                               DataPtr);
                // Tw added
                // To trigger SACC, check if the previous PIM unit has finished
                // (TODO) 비교하는 부분 수정 필요
                if(pim_index > 0){
                    int pim_index_SACC = pim_index - 1;
                    if(pim_index % 2 == 1){ //1,3,5,7... 만 연산할 수 있도록
                        if(shared_acc_[pim_index/2]->pim_unit_[pim_index%2]->enter_SACC == true \
                            && shared_acc_[pim_index_SACC/2]->pim_unit_[pim_index_SACC%2]->enter_SACC == true)
                        {
                            /*if (DebugMode(hex_addr)){
                                std::cout << " Pim_func_sim: Trigger SACC\n";
                                std::cout << " Pim index : " << pim_index << " Pim index SACC : " << pim_index_SACC << "\n";
                            }*/
                            // Send data from DRAM to L_IQ, R_IQ
                            if(addr.column % 2 == 0){
                                //왼쪽 홀수, 오른쪽 짝수
                                shared_acc_[pim_index/2]->loadIndices(hex_addr, shared_acc_[pim_index/2]->pim_unit_[pim_index%2]->bank_temp_, 
                                                                    shared_acc_[pim_index_SACC/2]->pim_unit_[pim_index_SACC%2]->bank_temp_);
                            }
                            else //다음 index로 넘어가기 위해 두개의 함수를 구분
                                shared_acc_[pim_index/2]->loadIndices_2(hex_addr, shared_acc_[pim_index/2]->pim_unit_[pim_index%2]->bank_temp_,
                                                                    shared_acc_[pim_index_SACC/2]->pim_unit_[pim_index_SACC%2]->bank_temp_);
                            shared_acc_[pim_index/2]->runSimulation(hex_addr);
                            shared_acc_[pim_index/2]->pim_unit_[pim_index%2]->enter_SACC = false;
                            shared_acc_[pim_index_SACC/2]->pim_unit_[pim_index_SACC%2]->enter_SACC = false;      
                            accumulation_count += shared_acc_[pim_index/2]-> accumulate_count;            
                        }
                    } 
                }
                // Change bankmode to PIM → AB when programmed μkernel is
                // finished and returns EXIT_END
                if (ret == EXIT_END) {
                    if (DebugMode(hex_addr)){
                        std::cout << " Pim_func_sim : PIM → AB mode change\n";
                    }
                    PIM_OP_MODE[addr.channel] = false;
                }
            }
        }
    }
}

} // namespace dramsim3