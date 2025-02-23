#include <iostream>
#include "./shared_acc.h"

#define MAX_QUEUE_SIZE 16

namespace dramsim3 {

SharedAccumulator::SharedAccumulator(Config &config, int id, PimUnit& pim1, PimUnit& pim2)
    : PimUnit(config, id),  // Call the PimUnit constructor
      SA_id(id),
      sa_clk(0), 
      L_Q_pop_cnt(0),
      R_Q_pop_cnt(0),
      config_(config) {
    // Add PimUnits to the vector
    pim_unit_.push_back(&pim1);
    pim_unit_.push_back(&pim2);

    column_data = (uint32_t*) malloc(WORD_SIZE);
    sa_clk = 0;
    column_index = 0;
    previous_column = 0;
}

void SharedAccumulator::init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
                   unsigned int burstSize) {
    pmemAddr_ = pmemAddr;
    pmemAddr_size_ = pmemAddr_size;
    burstSize_ = burstSize;
}

void SharedAccumulator::ReadColumn(uint64_t hex_addr) {
    // col = 0 번이 matrix의 column 정보를 담고 있음
    // (TODO) col = 0 에 대한 address mapping이 필요함
    memcpy(column_data, pmemAddr_ + hex_addr, WORD_SIZE);
}

uint64_t SharedAccumulator::ReverseAddressMapping(Address& addr) {
    uint64_t hex_addr = 0;
    hex_addr += (uint64_t)addr.channel << config_.ch_pos;
    hex_addr += (uint64_t)addr.rank << config_.ra_pos;
    hex_addr += (uint64_t)addr.bankgroup << config_.bg_pos;
    hex_addr += (uint64_t)addr.bank << config_.ba_pos;
    hex_addr += (uint64_t)addr.row << config_.ro_pos;
    hex_addr += (uint64_t)addr.column << config_.co_pos;
    return hex_addr << config_.shift_bits;
}

//Index Queue에 데이터를 넣는 함수
//MOV 명령어 시행시 DRAM row에서 데이터를 받아와야 함
//이때, DRAM row에서 받아온 데이터를 Index Queue에 넣어주는 함수
//L_indices와 R_indices는 각각 PimUnit 1과 연결된 Bansk PimUnit 2와 연결된 Bank에서 받아온 데이터
void SharedAccumulator::loadIndices(uint32_t *L_indices, uint32_t *R_indices) {
    //std::cout << "SA: Data loaded to Shared Accumulator ID: " << SA_id << std::endl;
    for (size_t i = 0; i < 8; i++) {
        //std::cout << " SA: L_indices[" << i << "]: " << L_indices[i]<<" ";
        if(L_indices[i] != 0){
            L_IQ.push(Element(i,L_indices[i]));
        }
    }
    //std::cout << std::endl;
    for (size_t i = 0; i < 8 /*R_indices.size()*/; i++) {
        //std::cout << "SA: R_indices[" << i << "]: " << R_indices[i] <<" ";
        if(R_indices[i] != 0){
            R_IQ.push(Element(i,R_indices[i]));
        }
    }
    //std::cout << "\n\n";
}

//Index Queue에 데이터를 넣는 함수2
//위와 설명은 동일 BUT 두 번째 SACC 실행시 index가 0 ~ 7이 아닌 8 ~ 15로 들어와야 됨
void SharedAccumulator::loadIndices_2(uint32_t *L_indices, uint32_t *R_indices) {
    //std::cout << "SA: Data loaded to Shared Accumulator ID: " << SA_id << std::endl;
    for (size_t i = 8; i < 15; i++) {
        //std::cout << " SA: L_indices[" << i << "]: " << L_indices[i-8]<<" ";
        if(L_indices[i] != 0){
            L_IQ.push(Element(i,L_indices[i]));
        }
    }
    //std::cout << std::endl;
    for (size_t i = 8; i < 15 /*R_indices.size()*/; i++) {
        //std::cout << "SA: R_indices[" << i << "]: " << R_indices[i-8] <<" ";
        if(R_indices[i] != 0){
            R_IQ.push(Element(i,R_indices[i]));
        }
    }
    //std::cout << "\n\n";
}


//Printx Clk 함수는 몇 클럭이 소모 될지 출력하는 함수
//sa_clk는 Shared Accumulator의 클럭 수를 나타냄
// (TODO) 아직 구현 X -> 일단은 필요 없어서 clk은 아직 안넣음
void SharedAccumulator::PrintClk(){
    std::cout << "Shared Accumulator ID: " << SA_id << " takes " << sa_clk << " clocks" << std::endl;
}

void SharedAccumulator::PrintElement(Element element){
    std::cout << "SA: Element Order: " << (uint32_t)element.order << " Value: " << element.value << std::endl;
    std::cout << "SA: Elemetn value: " << element.value << std::endl;
}

void SharedAccumulator::simulateStep() {
    if (!L_IQ.empty() && !R_IQ.empty()) {
        Element L_front = L_IQ.front();
        // TW added for debug
        //PrintElement(L_front);
        Element R_front = R_IQ.front();
        // TW added for debug
        //PrintElement(R_front);

        if ((L_front.value == R_front.value) && (L_front.value != 0 && R_front.value != 0)) {
            // Pop both queues
            L_IQ.pop();
            L_Q_pop_cnt++;
            R_IQ.pop();
            R_Q_pop_cnt++;

            // Signal load unit
            loadUnit(L_front.order, R_front.order); //order는 GRF를 access하기 위해 generate 된 index
        } else if (L_front.value > R_front.value) {
            //std::cout << "SA: MISS L is bigger than R\n";
            // Pop R_IQ
            R_IQ.pop();
            R_Q_pop_cnt++;
        } else {
            //std::cout << "SA: MISS R is bigger than L\n";
            // Pop L_IQ
            L_IQ.pop();
            L_Q_pop_cnt++;
        }
    }
    else{
        L_Q_pop_cnt = 0;
        R_Q_pop_cnt = 0;
    }
}

//아래의 structure는 PimUnit.h에 있음
/*struct Element {
    uint8_t order; //들어온 순서
    unit_t value; //값

    Element(int o, int v) : order(o), value(v) {}
};*/

// Simulation 상 LoadUnit은 REG R/W unit과 Adder controller 쌍으로 구성
// 두개의 역할을 loadunit이 처리
void SharedAccumulator::loadUnit(int index_l, int index_r) {

    std::cout << "SA: SA ID: " << SA_id << " Index Same " << std::endl;
    std::cout << "L_IQ.size : " << L_IQ.size() << " R_IQ.size : " << R_IQ.size() << std::endl;
    // Example: Load a value from GRF
    //uint64_t address = index * sizeof(int);  // Assuming an address mapping
    uint16_t data_l = 0;
    uint16_t data_r = 0;

    // Read data from GRF
    // Queue에서 받아온 정보를 기반으로, GRF의 index를 가져와야 됨)
    // (TODO) GRF_A에 access 할 떄 Index_l, index_r이 정상적인지 확인 필요
    data_l = pim_unit_[0]->GRF_A_[index_l];
    data_r = pim_unit_[1]->GRF_A_[index_r];
    pim_unit_[1]->GRF_A_[index_r] = 0; //한쪽 데이터는 0으로 바꿔야 됨

    // adder signal 및 더해서 다시
    pim_unit_[0]->GRF_A_[index_l] = data_l + data_r;
}

//TW added 2025.02.22
void SharedAccumulator::FlushQueue(){
    while(!L_IQ.empty())
        L_IQ.pop();
    while(!R_IQ.empty())
        R_IQ.pop();
}

void SharedAccumulator::runSimulation(uint64_t hex_addr) {
    #ifdef debug_mode
    //std::cout << "Shared Accumulator ID: " << SA_id << " simulation\n";
    #endif
    // Load index from DRAM bank
    //pim_unit_[0];
    //pim_unit_[1];
    uint32_t loop = 0;
    // ReadColumn 함수를 통해 column data를 읽어와 다른 column 일 때는 queue에 있는 데이터를
    // flush 하는 과정이 존재해야 됨
    Address addr = config_.AddressMapping(hex_addr);
    addr.column = 0;
    uint64_t hex_addr_col = ReverseAddressMapping(addr);
    //std::cout <<"previous column : " << previous_column << " current column : " << column_data[column_index] << std::endl;
    
    if(column_index != 0 && previous_column != column_data[column_index]){
        if(SA_id % 2 == 1){
            //std::cout<< "SA: SA ID: " << SA_id << " L_IQ is flushed\n";
            while(!L_IQ.empty())
                L_IQ.pop();
        }
        else{
            //std::cout<< "SA: SA ID: " << SA_id << " R_IQ is flushed\n";
            while(!R_IQ.empty())
                R_IQ.pop();
        }
    }
    previous_column = column_data[column_index];
    // Column 7개를 비교하기 위한 Index
    if(column_index < 7){
        column_index++;
    }
    else{
        column_index = 0;
    }

    //column index가 있는 곳을 잘 읽어 오고 있는 것을 확인
    
    ReadColumn(hex_addr_col); //4B x 7개와 empty 4B 1개
    // For debug // TW added
    /*for (int i = 0; i < 8; i++) {
        //std::cout << "column_data[" << i << "] : " << column_data[i] << std::endl;
        if(column_data[7] != 0){
            std::cerr << "SA: Column data is not empty\n";
            exit(1);
        }
    }*/
    while (!L_IQ.empty() || !R_IQ.empty()) {
        //std::cout << "L_IQ.size : " << L_IQ.size() << " R_IQ.size : " << R_IQ.size() << std::endl;
        //과정이 끝나고 flush 가 필요
        simulateStep();
        loop++;
        if(L_IQ.empty() || R_IQ.empty()){
            break;
        }
        if (loop > 100000) {
            std::cerr << "SA: Infinite loop detected\n";
            exit(1);
        }
    }

    //std::cout << "L_IQ.size : " << L_IQ.size() << " R_IQ.size : " << R_IQ.size() << std::endl;
    // TW added flush at 2025.02.22
    // QUEUE가 32개 이상이 되면 flush (64 일때 보다 성능이 좋게 나옴)
    if(L_IQ.size() > MAX_QUEUE_SIZE /2 || R_IQ.size() > MAX_QUEUE_SIZE /2){
        std::cout << "L_IQ.size : " << L_IQ.size() << " R_IQ.size : " << R_IQ.size() << std::endl;
        std::cout << "SA ID: " << SA_id << " L_IQ & R_IQ flushed\n";
        FlushQueue();
    }
    if(L_IQ.size() > MAX_QUEUE_SIZE || R_IQ.size() > MAX_QUEUE_SIZE){
        std::cerr << "SA ID: " << SA_id << " L_IQ or R_IQ is overflowed\n";
        exit(1);
    }
}

}  // namespace dramsim3