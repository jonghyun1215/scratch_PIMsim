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
}

//Index Queue에 데이터를 넣는 함수
//MOV 명령어 시행시 DRAM row에서 데이터를 받아와야 함
//이때, DRAM row에서 받아온 데이터를 Index Queue에 넣어주는 함수
//L_indices와 R_indices는 각각 PimUnit 1과 연결된 Bansk PimUnit 2와 연결된 Bank에서 받아온 데이터
void SharedAccumulator::loadIndices(std::vector<int> L_indices, std::vector<int> R_indices) {
    for (size_t i = 0; i < L_indices.size(); i++) {
        L_IQ.push(Element(i,L_indices[i]));
    }
    for (size_t i = 0; i < R_indices.size(); i++) {
        R_IQ.push(Element(i,R_indices[i]));
    }
}

//PrintClk 함수는 몇 클럭이 소모 될지 출력하는 함수
//sa_clk는 Shared Accumulator의 클럭 수를 나타냄
void SharedAccumulator::PrintClk(){
    std::cout << "Shared Accumulator ID: " << SA_id << " takes " << sa_clk << " clocks" << std::endl;
}

void SharedAccumulator::simulateStep() {
    if (!L_IQ.empty() && !R_IQ.empty()) {
        Element L_front = L_IQ.front();
        Element R_front = R_IQ.front();

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
            // Pop R_IQ
            R_IQ.pop();
            R_Q_pop_cnt++;
        } else {
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

void SharedAccumulator::loadUnit(int index) {
    // Example: Load a value from DRAM using DRAMsim3 and add to accumulator
    
    
    //uint64_t address = index * sizeof(int);  // Assuming an address mapping
    int data = 0;

    // Read data from GRF
    // Queue에서 받아온 정보를 기반으로, GRF의 index를 가져와야 됨
    
    if (index < 16) {
        data = pim_unit_[0]->GRF_A_[index];
    } else {
        data = pim_unit_[1]->GRF_A_[index];
    }

    // Assume the index is directly mapped to an accumulator
    accumulators[index % 8] += data;

    // Signal the adder controller (not shown, assuming addition happens here)
}

void SharedAccumulator::runSimulation() {
    while (!L_IQ.empty() || !R_IQ.empty()) {
        simulateStep();
    }}

}  // namespace dramsim3