#include <iostream>
#include "./shared_acc.h"

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

    std::fill(std::begin(accumulators), std::end(accumulators), 0);

    sa_clk = 0;
}

void SharedAccumulator::init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
                   unsigned int burstSize) {
    pmemAddr_ = pmemAddr;
    pmemAddr_size_ = pmemAddr_size;
    burstSize_ = burstSize;
}

//Index Queue에 데이터를 넣는 함수
//MOV 명령어 시행시 DRAM row에서 데이터를 받아와야 함
//이때, DRAM row에서 받아온 데이터를 Index Queue에 넣어주는 함수
//L_indices와 R_indices는 각각 PimUnit 1과 연결된 Bansk PimUnit 2와 연결된 Bank에서 받아온 데이터
void SharedAccumulator::loadIndices(uint32_t *L_indices, uint32_t *R_indices) {
    std::cout << "SA: Data loaded to Shared Accumulator ID: " << SA_id << std::endl;
    for (size_t i = 0; i < 8; i++) {
        //std::cout << " SA: L_indices[" << i << "]: " << L_indices[i]<<" ";
        L_IQ.push(Element(i,L_indices[i]));
    }
    //std::cout << std::endl;
    for (size_t i = 0; i < 8 /*R_indices.size()*/; i++) {
        //std::cout << "SA: R_indices[" << i << "]: " << R_indices[i] <<" ";
        R_IQ.push(Element(i,R_indices[i]));
    }
    std::cout << "\n\n";
}

//Index Queue에 데이터를 넣는 함수2
//위와 설명은 동일 BUT 두 번째 SACC 실행시 index가 0 ~ 7이 아닌 8 ~ 15로 들어와야 됨
void SharedAccumulator::loadIndices_2(uint32_t *L_indices, uint32_t *R_indices) {
    std::cout << "SA: Data loaded to Shared Accumulator ID: " << SA_id << std::endl;
    for (size_t i = 8; i < 15; i++) {
        //std::cout << " SA: L_indices[" << i << "]: " << L_indices[i-8]<<" ";
        L_IQ.push(Element(i,L_indices[i]));
    }
    //std::cout << std::endl;
    for (size_t i = 8; i < 15 /*R_indices.size()*/; i++) {
        //std::cout << "SA: R_indices[" << i << "]: " << R_indices[i-8] <<" ";
        R_IQ.push(Element(i,R_indices[i]));
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
        PrintElement(L_front);
        Element R_front = R_IQ.front();
        // TW added for debug
        PrintElement(R_front);

        if (L_front.value == R_front.value) {
            // Pop both queues
            L_IQ.pop();
            L_Q_pop_cnt++;
            R_IQ.pop();
            R_Q_pop_cnt++;

            // Signal load unit
            loadUnit(L_front.order); //order는 GRF를 access하기 위해 generate 된 index
            loadUnit(R_front.order); //order는 GRF를 access하기 위해 generate 된 index
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
void SharedAccumulator::loadUnit(int index) {

    std::cout << "SA: SA ID: " << SA_id << " Index Same " << std::endl;
    // Example: Load a value from GRF
    //uint64_t address = index * sizeof(int);  // Assuming an address mapping
    uint16_t data_l = 0;
    uint16_t data_r = 0;

    // Read data from GRF
    // Queue에서 받아온 정보를 기반으로, GRF의 index를 가져와야 됨
    data_l = pim_unit_[0]->GRF_A_[index];
    data_r = pim_unit_[1]->GRF_A_[index];
    pim_unit_[1]->GRF_A_[index] = 0; //한쪽 데이터는 0으로 바꿔야 됨

    // adder signal 및 더해서 다시
    pim_unit_[0]->GRF_A_[index] = data_l + data_r;
}

void SharedAccumulator::runSimulation() {
    #ifdef debug_mode
    std::cout << "Shared Accumulator ID: " << SA_id << " starts simulation\n";
    #endif
    // Load index from DRAM bank
    //pim_unit_[0];
    //pim_unit_[1];
    uint32_t loop = 0;
    while (!L_IQ.empty() || !R_IQ.empty()) {
        std::cout << "L_IQ.size : " << L_IQ.size() << " R_IQ.size : " << R_IQ.size() << std::endl;
        simulateStep();
        loop++;
        if (loop > 100) {
            std::cerr << "SA: Infinite loop detected\n";
            exit(1);
        }
    }
    std::cout << "Shared Accumulator ID: " << SA_id << " ends simulation\n";
    }
}  // namespace dramsim3