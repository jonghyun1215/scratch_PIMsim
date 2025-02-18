#include "./pim_unit.h"
#include <iostream>

using half_float::half;

namespace dramsim3 {

PimUnit::PimUnit(Config &config, int id)
  : pim_id(id),
    config_(config)
{
    PPC = 0;  // PIM program counter : Points the PIM Instruction to execute in
              //                       CRF register
    LC  = 0;  // Loop counter : A counter to perform NOP, JUMP Instructions

    // Initialize PIM Registers
    // TW added
    // unit_t = uint16_t
    GRF_A_ = (unit_t*) malloc(GRF_SIZE); //256B = 32B * 8개
    GRF_B_ = (unit_t*) malloc(GRF_SIZE);
    SRF_A_ = (unit_t*) malloc(SRF_SIZE);
    SRF_M_ = (unit_t*) malloc(SRF_SIZE); //16B
    bank_data_ = (unit_t*) malloc(WORD_SIZE);
    dst = (unit_t*) malloc(WORD_SIZE);

    for (int i=0; i< WORD_SIZE / (int)sizeof(unit_t); i++) 
        dst[i] = 0;
    for (int i=0; i< GRF_SIZE / (int)sizeof(unit_t); i++) {
        GRF_A_[i] = 0; //각 인덱스로 16Byte access 가능 한 것을 확인
        GRF_B_[i] = 0;
    }

    // SRF_SIZE = 16B
    // size of uint16_t = 2B
    // i=0 ~ 8
    for (int i=0; i< SRF_SIZE / (int)sizeof(unit_t); i++) {
        SRF_A_[i] = 0;
        SRF_M_[i] = 0; //8개의 데이터가 들어감
    }
    
    //TW added to support SACC
    enter_SACC = false;
    bank_temp_ = (uint32_t*) malloc(WORD_SIZE);
}

void PimUnit::init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
                   unsigned int burstSize) {
    pmemAddr_ = pmemAddr;
    pmemAddr_size_ = pmemAddr_size;
    burstSize_ = burstSize;
}

// Return to print out debugging information or not
//  Can set debug_mode and watch_pimindex at pim_config.h
bool PimUnit::DebugMode() {
    #ifndef debug_mode
    return false;
    #endif

    if (pim_id == watch_pimindex) return true;
    return false;
}

// Print operand's type (register type, Bank)
void PimUnit::PrintOperand(int op_id) {
    if (op_id == 0) std::cout << "BANK";
    else if (op_id == 1) std::cout << "GRF_A";
    else if (op_id == 2) std::cout << "GRF_B";
    else if (op_id == 3) std::cout << "SRF_A";
    else if (op_id == 4) std::cout << "SRF_M";
}

// Print Instruction according to PIM operation id
void PimUnit::PrintPIM_IST(PimInstruction inst) { //PIM Config.h 파일의 PIM_OPERATION 참조
    if (inst.PIM_OP == (PIM_OPERATION)0) std::cout << "NOP\t";
    else if (inst.PIM_OP == (PIM_OPERATION)1) std::cout << "JUMP\t";
    else if (inst.PIM_OP == (PIM_OPERATION)2) std::cout << "EXIT\t";
    else if (inst.PIM_OP == (PIM_OPERATION)4) std::cout << "MOV\t";
    else if (inst.PIM_OP == (PIM_OPERATION)5) std::cout << "FILL\t";
    else if (inst.PIM_OP == (PIM_OPERATION)8) std::cout << "ADD\t";
    else if (inst.PIM_OP == (PIM_OPERATION)9) std::cout << "MUL\t";
    else if (inst.PIM_OP == (PIM_OPERATION)10) std::cout << "MAC\t";
    else if (inst.PIM_OP == (PIM_OPERATION)11) std::cout << "MAD\t";
    else if (inst.PIM_OP == (PIM_OPERATION)12) std::cout << "SACC\t"; //TW added
    else std::cout << "UNKNOWN\t";

    if (inst.pim_op_type == (PIM_OP_TYPE)0) {  // CONTROL
        std::cout << (int)inst.imm0 << "\t";
        std::cout << (int)inst.imm1 << "\t";
    } else if (inst.pim_op_type == (PIM_OP_TYPE)1) {  // DATA
        PrintOperand((int)inst.dst);
        if ((int)inst.dst != 0) {
            if (inst.is_aam == 0 || inst.is_dst_fix)  std::cout << "[" << inst.dst_idx << "]";
            else std::cout << "(A)";
        }
        std::cout << "  ";

        PrintOperand((int)inst.src0);
        if ((int)inst.src0 != 0) {
            if (inst.is_aam == 0 || inst.is_src0_fix)  std::cout << "[" << inst.src0_idx << "]";
            else std::cout << "(A)";
        }
        std::cout << "  ";
    } else if(inst.pim_op_type == (PIM_OP_TYPE)2) {  // ALU
        PrintOperand((int)inst.dst);
        if ((int)inst.dst != 0) {
            if (inst.is_aam == 0 || inst.is_dst_fix)  std::cout << "[" << inst.dst_idx << "]";
            else std::cout << "(A)";
        }
        std::cout << "  ";
        PrintOperand((int)inst.src0);
        if ((int)inst.src0 != 0) {
            if (inst.is_aam == 0 || inst.is_src0_fix)  std::cout << "[" << inst.src0_idx << "]";
            else std::cout << "(A)";
        }
        std::cout << "  ";
        PrintOperand((int)inst.src1);
        if ((int)inst.src1 != 0) {
            if (inst.is_aam == 0 || inst.is_src1_fix)  std::cout << "[" << inst.src1_idx << "]";
            else std::cout << "(A)";
        }
        std::cout << "  ";
    }
    //TW added
    else if(inst.pim_op_type == (PIM_OP_TYPE)3) {  // SACC
        PrintOperand((int)inst.src0);
        if ((int)inst.src0 != 0) {
            if (inst.is_aam == 0 || inst.is_src0_fix)  std::cout << "[" << inst.src0_idx << "]";
            else std::cout << "(A)";
        }
        std::cout << "  ";
    }
    std::cout << "\n";
}

// Set pim_unit's SRF Register
//  Data in DataPtr will be 32-byte data
//  Front 16-byte of DataPtr is written to SRF_A Register and
//  next 16-byte of DataPtr is written to SRF_M Register
//  외부에서 R/W로 SRF에 데이터를 쓰기 위해 구현된 함수
void PimUnit::SetSrf(uint64_t hex_addr, uint8_t* DataPtr) {
    if (DebugMode()) std::cout << " PU: SetSrf\n";
    memcpy(SRF_A_, DataPtr, SRF_SIZE);
    memcpy(SRF_M_, DataPtr + SRF_SIZE, SRF_SIZE);
}

// Set pim_unit's GRF Register
//  Data in DataPtr will be 32-byte bata
//  if hex_addr.Column Address is 0~7
//  Column Address 0~7 data is written to GRF_A 0~7 each
//  if hex_addr.Column Address is 8~15
//  Column Address 8~15 data is written to GRF_B 0~7 each
void PimUnit::SetGrf(uint64_t hex_addr, uint8_t* DataPtr) {
    if (DebugMode()) std::cout << "  PU: SetGrf\n";
    Address addr = config_.AddressMapping(hex_addr);
    if (addr.column < 8) {  // GRF_A
        unit_t* target = GRF_A_ + addr.column *WORD_SIZE / sizeof(unit_t);
        memcpy(target, DataPtr, WORD_SIZE); //WORD_SIZE = 32Byte
    } else {  // GRF_B
        GRF_B_[15] = 0;
        unit_t* target = GRF_B_ + (addr.column-8) *WORD_SIZE / sizeof(unit_t);
        memcpy(target, DataPtr, WORD_SIZE);
    }
}

// Set pim_unit's CRF Register
//  32bit inst x 8 = 32Byte data 
//  Column Address 0 data is written to CRF 0~7
//  Column Address 1 data is written to CRF 8~15
//  Column Address 2 data is written to CRF 16~23
//  Column Address 3 data is written to CRF 24~31
//  바이트 단위로 access 되고 있기 때문에 위와 같은 구조로 구성
void PimUnit::SetCrf(uint64_t hex_addr, uint8_t* DataPtr) {
    if (DebugMode()) std::cout << "  PU: SetCrf\n";
    Address addr = config_.AddressMapping(hex_addr);
    int CRF_idx = addr.column * 8;
    for (int i=0; i< 8; i++) {
        PushCrf(CRF_idx+i, DataPtr + 4*i);
    }
}

// Map 32-bit data into structure of PIM_INSTRUCTION
void PimUnit::PushCrf(int CRF_idx, uint8_t* DataPtr) {
    CRF[CRF_idx].PIM_OP = BitToPIM_OP(DataPtr);
    CRF[CRF_idx].is_aam = CheckAam(DataPtr);
    CRF[CRF_idx].is_dst_fix = CheckDstFix(DataPtr);
    CRF[CRF_idx].is_src0_fix = CheckSrc0Fix(DataPtr);
    CRF[CRF_idx].is_src1_fix = CheckSrc1Fix(DataPtr);

    switch (CRF[CRF_idx].PIM_OP) {
        case PIM_OPERATION::ADD: //ADD를 지원하기 위해서는 8개로 변경 필요 //TW added
        case PIM_OPERATION::MUL:
        case PIM_OPERATION::MAC:             
        case PIM_OPERATION::MAD:
            CRF[CRF_idx].pim_op_type = PIM_OP_TYPE::ALU;
            CRF[CRF_idx].dst  = BitToDst(DataPtr);
            CRF[CRF_idx].src0 = BitToSrc0(DataPtr);
            CRF[CRF_idx].src1 = BitToSrc1(DataPtr);
            CRF[CRF_idx].dst_idx  = BitToDstIdx(DataPtr);
            CRF[CRF_idx].src0_idx = BitToSrc0Idx(DataPtr);
            CRF[CRF_idx].src1_idx = BitToSrc1Idx(DataPtr);
            break;
        case PIM_OPERATION::MOV:
        case PIM_OPERATION::FILL:
            CRF[CRF_idx].pim_op_type = PIM_OP_TYPE::DATA;
            CRF[CRF_idx].dst  = BitToDst(DataPtr);
            CRF[CRF_idx].src0 = BitToSrc0(DataPtr);
            CRF[CRF_idx].src1 = BitToSrc1(DataPtr);
            CRF[CRF_idx].dst_idx  = BitToDstIdx(DataPtr);
            CRF[CRF_idx].src0_idx = BitToSrc0Idx(DataPtr);
            break;
        case PIM_OPERATION::NOP:
            CRF[CRF_idx].pim_op_type = PIM_OP_TYPE::CONTROL;
            //TW added
            //NOP를 몇번 반복할지에 대한 정보를 저장하기위해 imm1 사용
            CRF[CRF_idx].imm1 = BitToImm1(DataPtr);
            break;
        case PIM_OPERATION::JUMP:
            CRF[CRF_idx].imm0 = CRF_idx + BitToImm0(DataPtr);
            CRF[CRF_idx].imm1 = BitToImm1(DataPtr);
            CRF[CRF_idx].pim_op_type = PIM_OP_TYPE::CONTROL;
            break;
        case PIM_OPERATION::EXIT:
            CRF[CRF_idx].pim_op_type = PIM_OP_TYPE::CONTROL;
            break;
        //TW added
        // MOV, FILL과 유사하게 구현
        case PIM_OPERATION::SACC:
            CRF[CRF_idx].pim_op_type = PIM_OP_TYPE::SHARED;
            // src0이 BANK를 나타내기 위해 사용
            CRF[CRF_idx].src0 = BitToSrc0(DataPtr);
            CRF[CRF_idx].src0_idx = BitToSrc0Idx(DataPtr);
            //std::cout << "CRF[CRF_idx].src0_idx : " << CRF[CRF_idx].src0_idx << std::endl;
            break;
        default:
            break;
    }
    if (DebugMode()) {
        std::cout << "  PU: program  ";
        PrintPIM_IST(CRF[CRF_idx]);
    }
}

// Execute PIM_INSTRUCTIONS in CRF register and compute PIM
// PIM Unit에서 자체적으로 데이터를 가져오기 위해 만들어진 함
int PimUnit::AddTransaction(uint64_t hex_addr, bool is_write,
                            uint8_t* DataPtr) {
    // Read data from physical memory
    // Read 명령어 도착시, PMEM에서 데이터를 읽어와서 bank_data_에 저장
    //hex_addr의 32B 데잍터를 bank_data_에 저장
    if (!is_write)
        memcpy(bank_data_ , pmemAddr_ + hex_addr, WORD_SIZE); 

    // Map operand data's offset to computation pointers properly
    SetOperandAddr(hex_addr);

    // Execute PIM_INSTRUCTION
    // Is executed using computation pointers mapped from SetOperandAddr
    Execute();

    // if PIM_INSTRUCTION that writes data to physical memory
    // is executed, write to physcial memory
    if (CRF[PPC].PIM_OP == PIM_OPERATION::MOV &&
        CRF[PPC].dst == PIM_OPERAND::BANK) {
        // 여기서 새롭게 추가한 MOV 명령어가 Bank에 32B 데이터를 씀
        memcpy(pmemAddr_ + hex_addr, dst, WORD_SIZE);
    }

    //TW added
    // MOV를 이용하여, SRF를 채우는 경우
    // 기존에는 지원하지 않았음
    // 상위 16B는 SRF_M에, 하위 16B는 SRF_A에 저장
    if (CRF[PPC].PIM_OP == PIM_OPERATION::MOV &&
        CRF[PPC].dst == PIM_OPERAND::SRF_M) {
        memcpy(SRF_M_, pmemAddr_ + hex_addr, SRF_SIZE);
        memcpy(SRF_A_, pmemAddr_ + hex_addr + SRF_SIZE, SRF_SIZE);
    }
    
    // Point to next PIM_INSTRUCTION
    PPC += 1; //PPC= PIM Program Counter

    // Deal with PIM operation NOP & JUMP
    //  Performed by using LC(Loop Counter)
    //  LC copies the number of iterations and gets lower by 1 when executed
    //  Repeats until LC gets to 1 and escapes the iteration
    if (CRF[PPC].PIM_OP == PIM_OPERATION::NOP) {
        if (LC == 0) {
            LC = CRF[PPC].imm1;
        } else if (LC > 1) {
            LC -= 1;
        } else if (LC == 1) {
            PPC += 1;
            LC = 0;
            return NOP_END;
        }
        if (DebugMode()) {
            std::cout << "  PU: NOP left (" << LC << ")\n";
        }
        return 0;
    } else if (CRF[PPC].PIM_OP == PIM_OPERATION::JUMP) {
        if (LC == 0) {
            LC = CRF[PPC].imm1;
            PPC = CRF[PPC].imm0;
        } else if (LC > 1) {
            PPC = CRF[PPC].imm0;
            LC -= 1;
        } else if (LC == 1) {
            PPC += 1;
            LC = 0;
        }
        if (DebugMode()) {
            std::cout << "  PU: JUMP left (" << LC << ")\n";
        }
    }

    // When pointed PIM_INSTRUCTION is EXIT, μkernel is finished
    // Reset PPC and return EXIT_END
    if (CRF[PPC].PIM_OP == PIM_OPERATION::EXIT) {
        if (DebugMode()) {
            std::cout << "  PU: EXIT\n";
        }
        PPC = 0;

        return EXIT_END;
    }

    return 0;  // NORMAL_END
}

// Map operand data's offset to computation pointers properly
// AAM mode is controlled in this function
void PimUnit::SetOperandAddr(uint64_t hex_addr) {
    // set _GRF_A, _GRF_B operand address when AAM mode
    Address addr = config_.AddressMapping(hex_addr);
    if (CRF[PPC].is_aam) { // Address Aligned Mode
        //ROW랑 COLUMN을 이용해서 AAM을 구현
        int ADDR = addr.row * 32 + addr.column; //Column 0~31
        int dst_idx = int(ADDR / pow(2, CRF[PPC].dst_idx)) % 8;
        int src0_idx = int(ADDR / pow(2, CRF[PPC].src0_idx)) % 8;
        int src1_idx = int(ADDR / pow(2, CRF[PPC].src1_idx)) % 8;
        
        // TW added
        // HARD CODED
        // SACC 명령이 있을 경우 MOV를 이용하여 GRF_A에서 BANK로 넘어가는 명령어가 있는 경우
        // src0_idx를 강제적으로 맞춰줌
        bool SACC_flag = false;
        for (int i=0;i<31;i++)
        {
            if (CRF[i].PIM_OP == PIM_OPERATION::SACC)
            {
                SACC_flag = true;
                break;
            }
        }
        if (CRF[PPC].PIM_OP == PIM_OPERATION::MOV && SACC_flag)
        {
            src0_idx += 2;
            src0_idx = src0_idx % 8;
        }
        // TW hard coded end

        /*
        // TW added for debugging
        if(CRF[PPC].PIM_OP == PIM_OPERATION::SACC){
            if(dst_idx == src0_idx && dst_idx == src1_idx && src0_idx == src1_idx){
                std::cout << "All idx are same\n";
            }
        }*/
        // std::cout << A_idx << " " << B_idx << " " << dst_idx << " " << src0_idx << " " << src1_idx << std::endl;
       
        // int CA = addr.column;
        // int RA = addr.row;
        // int A_idx = CA % 8;
        // int B_idx = CA / 8 + RA % 2 * 4;
         
        // set dst address (AAM)
        if (CRF[PPC].dst == PIM_OPERAND::GRF_A) {
            if (CRF[PPC].is_dst_fix) { 
                dst = GRF_A_ + CRF[PPC].dst_idx * 16;
            } else { 
                dst = GRF_A_ + dst_idx * 16;
            }
        } else if (CRF[PPC].dst == PIM_OPERAND::GRF_B) {
            if (CRF[PPC].is_dst_fix) {
                dst = GRF_B_ + CRF[PPC].dst_idx * 16;
            } else { 
                dst = GRF_B_ + dst_idx * 16;
            }
        }
        // TW added
        // SRF_A, SRF_M의 경우에 대한 처리 필요
        // 기존에는 GRF_A, GRF_B만 처리하고 있었음
        // SRF_M 과 SRF_A는 16B 단위
        // corrupted size vs. prev_size  에러 발생
        /*else if(CRF[PPC].dst == PIM_OPERAND::SRF_M){
            dst = SRF_M_ + dst_idx;
            std::cout << "PU: SRF_M selected\n";
        }
        else if(CRF[PPC].dst == PIM_OPERAND::SRF_A){
            dst = SRF_A_ + dst_idx;
            std::cout << "PU: SRF_A selected\n";
            exit(1);
        } */

        // set src0 address (AAM) Address Aligned Mode
        // (TODO) GRF_A에서 BANK로 잘 들어가는지 확인 필요
        if (CRF[PPC].src0 == PIM_OPERAND::GRF_A) {
            if (CRF[PPC].is_src0_fix) { 
                src0 = GRF_A_ + CRF[PPC].src0_idx * 16;
            } else {
                // 새롭게 추가된 MOV의 경우 여기를 통해 들어감 -> 잘 들어가는 것 확인
                //if(CRF[PPC].PIM_OP == PIM_OPERATION::MOV)
                //    std::cout << "MOV TEST2: src0_idx = " << src0_idx << "PU id = " << pim_id << std::endl;
                src0 = GRF_A_ + src0_idx * 16; 

            }
        } else if (CRF[PPC].src0 == PIM_OPERAND::GRF_B) {
            if (CRF[PPC].is_src0_fix) {
                src0 = GRF_B_ + CRF[PPC].src0_idx * 16;
            } else {
                src0 = GRF_B_ + src0_idx * 16;
            }
        } else if (CRF[PPC].src0 == PIM_OPERAND::SRF_A) {
            if (CRF[PPC].is_src0_fix) {
                src0 = SRF_A_ + CRF[PPC].src0_idx * 16;
            } else {
                src0 = SRF_A_ + src0_idx;
            }
        }
        else if (CRF[PPC].src0 == PIM_OPERAND::SRF_M) {
            if (CRF[PPC].is_src0_fix) {
                src0 = SRF_M_ + CRF[PPC].src0_idx * 16;
            } else {
                src0 = SRF_M_ + src0_idx;
            }
        }

        // set src1 address (AAM)
        if (CRF[PPC].src1 == PIM_OPERAND::GRF_A) {
            if (CRF[PPC].is_src1_fix) {
                src1 = GRF_A_ + CRF[PPC].src1_idx * 16;
            } else {
                src1 = GRF_A_ + src1_idx * 16;
            }
        } else if (CRF[PPC].src1 == PIM_OPERAND::GRF_B) {
            if (CRF[PPC].is_src1_fix) {
                src1 = GRF_B_ + CRF[PPC].src1_idx * 16;
            } else {
                src1 = GRF_B_ + src1_idx * 16;
            }
        } else if (CRF[PPC].src1 == PIM_OPERAND::SRF_A) {
            if (CRF[PPC].is_src1_fix) {
                src1 = SRF_A_ + CRF[PPC].src1_idx * 16;
            } else {
                src1 = SRF_A_ + src1_idx;
            }
        // (TODO) SRF_M의 경우에 대한 처리 필요
        // 내 코드의 경우 src1이 fix 되지 않음
        // 여기로만 들어가서 SOURCE OPERAND 로 사용
        } else if (CRF[PPC].src1 == PIM_OPERAND::SRF_M) {
            if (CRF[PPC].is_src1_fix) {
                //std::cout << "TEST\n";
                src1 = SRF_M_ + CRF[PPC].src1_idx * 16;
            } else {
                //여기로만 들어감
                // (TODO) src1_idx 값이 AA에 따라 변화 해버림
                if(CRF[PPC].PIM_OP == PIM_OPERATION::MUL){
                    src1 = SRF_M_ + src1_idx-1;
                }
                else{
                    src1 = SRF_M_ + src1_idx;
                }
            }
        }
    } else {      // set _GRF_A, _GRF_B operand address when non-AAM mode
        // set dst address
        if (CRF[PPC].dst == PIM_OPERAND::GRF_A)
            dst = GRF_A_ + CRF[PPC].dst_idx * 16;
        else if (CRF[PPC].dst == PIM_OPERAND::GRF_B)
            dst = GRF_B_ + CRF[PPC].dst_idx * 16;

        // set src0 address
        if (CRF[PPC].src0 == PIM_OPERAND::GRF_A)
            src0 = GRF_A_ + CRF[PPC].src0_idx * 16;
        else if (CRF[PPC].src0 == PIM_OPERAND::GRF_B)
            src0 = GRF_B_ + CRF[PPC].src0_idx * 16;
        else if (CRF[PPC].src0 == PIM_OPERAND::SRF_A)
            src1 = SRF_A_ + CRF[PPC].src1_idx;
        //TW added

        // set src1 address
        // PIM_OP == ADD, MUL, MAC, MAD -> uses src1 for operand
        if (CRF[PPC].pim_op_type == PIM_OP_TYPE::ALU) {
            if (CRF[PPC].src1 == PIM_OPERAND::GRF_A)
                src1 = GRF_A_ + CRF[PPC].src1_idx * 16;
            else if (CRF[PPC].src1 == PIM_OPERAND::GRF_B)
                src1 = GRF_B_ + CRF[PPC].src1_idx * 16;
            else if (CRF[PPC].src1 == PIM_OPERAND::SRF_A)
                src1 = SRF_A_ + CRF[PPC].src1_idx;
            // 여기로는 들어가지 않음
            else if (CRF[PPC].src1 == PIM_OPERAND::SRF_M)
                src1 = SRF_M_ + CRF[PPC].src1_idx;
        }
    }

    // set BANK, operand address
    // . set dst address
    if (CRF[PPC].dst == PIM_OPERAND::BANK){
        dst = bank_data_;
        // (TODO) 여기를 통해 데이터가 들어갈 것이라고 예상 BUT 그러지 않음
        // MOV 명령어로 MOV BANK GRF_A를 할 경우 여기도 데이터가 들어감
    }

    // . set src0 address
    // MOV 명령어시 여기서 bank_data -> src0으로 데이터를 가져옴
    if (CRF[PPC].src0 == PIM_OPERAND::BANK){
        src0 = bank_data_;
    }

    // . set src1 address only if PIM_OP_TYPE == ALU
    //   -> uses src1 for operand
    if (CRF[PPC].pim_op_type == PIM_OP_TYPE::ALU)
        if (CRF[PPC].src1 == PIM_OPERAND::BANK)
            src1 = bank_data_;
}

// Execute PIM_INSTRUCTION
void PimUnit::Execute() {
    if (DebugMode()) {
        std::cout << "  PU: execute  ";
        PrintPIM_IST(CRF[PPC]);
    }
    switch (CRF[PPC].PIM_OP) {
        case PIM_OPERATION::ADD:
            _ADD();
            break;
        case PIM_OPERATION::MUL:
            _MUL();
            break;
        case PIM_OPERATION::MAC:
            _MAC();
            break;
        case PIM_OPERATION::MAD:
            _MAD();
            break;
        case PIM_OPERATION::MOV:
        case PIM_OPERATION::FILL:
            _MOV();
            break;
        // TW added
        case PIM_OPERATION::SACC: //여기 진입 잘 하는 것 확인
            _SACC();
            break;
        default:
            break;
    }
}

// TW added
void PimUnit::_SACC() {
    //UNITS_PER_WORD = 1
    for (int i = 0; i < UNITS_PER_WORD / 2; i++) {
        //2개의 2바이트 데이터를 하나의 4바이트 데이터로 합침
        //16 bit 두개를 합쳐 32 bit로 만들어줌
        bank_temp_[i] = ((uint32_t)src0[2*i+1] << 16) | (uint32_t)src0[2*i];
        //std::cout << "TW PU ID: " << pim_id << " PU: bank_temp_[" << i << "]: " << bank_temp_[i] << std::endl;
    }
    //TW added to support SACC
    enter_SACC = true;
}

void PimUnit::_ADD() {
    //std::cout << "PIMUnit::_ADD" << std::endl;
    if (CRF[PPC].src1 == PIM_OPERAND::SRF_A) {
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            half h_src0(*reinterpret_cast<half*>(&src0[i]));
            half h_src1(*reinterpret_cast<half*>(&src1[0]));
            half h_dst = h_src0 + h_src1;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    } else {
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            half h_src0(*reinterpret_cast<half*>(&src0[i]));
            half h_src1(*reinterpret_cast<half*>(&src1[i]));
            half h_dst = h_src0 + h_src1;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    }
}

void PimUnit::_MUL() {
    //std::cout << "PIMUnit::_MUL" << std::endl;
    // 아래는 기존의 코드
    /*if(CRF[PPC].src1 == PIM_OPERAND::SRF_M) {
        for (int i = 0; i < UNITS_PER_WORD; i++) {  
            half h_src0(*reinterpret_cast<half*>(&src0[i]));
            //아래는 SRF_M에서 읽어옴
            half h_src1(*reinterpret_cast<half*>(&src1[0]));
            half h_dst = h_src0 * h_src1;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    } else {
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            half h_src0(*reinterpret_cast<half*>(&src0[i]));
            half h_src1(*reinterpret_cast<half*>(&src1[i]));
            half h_dst = h_src0 * h_src1;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    }*/
   // TW added
   // unit_t 는 uint16_t, fp 16이나, 시뮬레이터에서는 uint16_t로 사용
   if(CRF[PPC].src1 == PIM_OPERAND::SRF_M) {
        // UNITES_PER_WORD = 16
        for (int i = 0; i < UNITS_PER_WORD; i++) {  
            unit_t h_src0(*reinterpret_cast<unit_t*>(&src0[i]));
            //아래는 SRF_M에서 읽어옴
            unit_t h_src1(*reinterpret_cast<unit_t*>(&src1[0]));
            //if(h_src0 == 1 && h_src1 == 0)
            //    std::cout<<"PU: h_src0[" <<i<< "]: " << h_src0 <<" h_src1(SRF_M): "<<h_src1<<std::endl;
            unit_t h_dst = h_src0 * h_src1;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    } else {
        //std::cout << "ERROR: Selecting this operand is not supported\n";
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            unit_t h_src0(*reinterpret_cast<unit_t*>(&src0[i]));
            unit_t h_src1(*reinterpret_cast<unit_t*>(&src1[i]));
            unit_t h_dst = h_src0 * h_src1;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    }
}

void PimUnit::_MAC() {
    //std::cout << "PIMUnit::_MAC" << std::endl;
    if (CRF[PPC].src1 == PIM_OPERAND::SRF_M) {
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            half h_dst(*reinterpret_cast<half*>(&dst[i]));
            half h_src0(*reinterpret_cast<half*>(&src0[i]));
            half h_src1(*reinterpret_cast<half*>(&src1[0]));
            h_dst = fma(h_src0, h_src1, h_dst);
            //std::cout << h_dst << " " << h_src0 << " " << h_src1 << std::endl;
            //std::cout << "(MAC) GRF_B[0]: " << (int)GRF_B_[0] << std::endl;
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    } else {
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            half h_dst(*reinterpret_cast<half*>(&dst[i]));
            half h_src0(*reinterpret_cast<half*>(&src0[i]));
            half h_src1(*reinterpret_cast<half*>(&src1[i]));
            h_dst = fma(h_src0, h_src1, h_dst);
            dst[i] = *reinterpret_cast<unit_t*>(&h_dst);
        }
    }
}
void PimUnit::_MAD() {
    std::cout << "not yet\n";
}

void PimUnit::_MOV() {
    //std::cout << "(MOV) GRF_B[0]: " << (int)GRF_B_[0] << std::endl;
    // UNITES_PER_WORD = 16
    // SetSrf를 이용하면 32Byte 데이터가 SRF_A(8개), SRF_M(8개) 순서로 저장됨)
    if(CRF[PPC].dst == PIM_OPERAND::SRF_M){
        // UNITES_PER_WORD = 16
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            if (i < UNITS_PER_WORD / 2) {
                SRF_M_[i] = src0[i];
            } else {
                int t = i - UNITS_PER_WORD / 2;
                SRF_A_[t] = src0[i];
            }
        }
    }
    else{
        for (int i = 0; i < UNITS_PER_WORD; i++) {
            // TW added to debug
            //std::cout <<"MOV " <<i << ": " << dst[i] << " " << src0[i] << std::endl;
            dst[i] = src0[i]; //32B data가 이동 0~15 * 2B
        }
    }
}

}  // namespace dramsim3
