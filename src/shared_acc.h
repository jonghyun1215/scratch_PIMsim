#ifndef SHARED_ACC_H
#define SHARED_ACC_H

#include <iostream>
#include <queue>
#include <vector>
#include <array>  // 배열 사용
#include "pim_unit.h"  // Include the PimUnit class
#include "configuration.h"
#include "common.h"
#include "pim_config.h"
#include "pim_utils.h"

namespace dramsim3 {

//Queue에 index 정보를 포함하고 있어야 될거 같아
//생성한 Struct
//실제 논문에서는 input된 순서가 결국 GRF의 index이기 때문에 별도의
//index를 저장할 필요가 없다고 얘기하면 될 듯
struct Element {
    uint8_t order;
    unit_t value;

    Element(int o, int v) : order(o), value(v) {}
};

// SharedAccumulator inherits from PimUnit
class SharedAccumulator : public PimUnit {
public:
    std::queue<Element> L_IQ;
    std::queue<Element> R_IQ;

    // Constructor
    SharedAccumulator(Config &config, int id, PimUnit& pim1, PimUnit& pim2);

    // Additional methods
    void loadIndices(uint64_t hex_addr, uint32_t *L_indices, uint32_t *R_indices);
    //새롭게 하위 8개의 index를 받아오기 위해 추가 됨
    void loadIndices_2(uint64_t hex_addr, uint32_t *L_indices, uint32_t *R_indices);
    void simulateStep();
    void loadUnit(int index_l, int index_r);
    void runSimulation(uint64_t hex_addr);
    void PrintClk();
    void init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
              unsigned int burstSize);
    void PrintElement(Element element);

    //TW added 2025.02.22
    void FlushQueue(); //To flush all values in the queue

    // DRAM bank로 부터 column data를 읽어오는 함수
    void ReadColumn(uint64_t hex_addr);
    uint64_t ReverseAddressMapping(Address& addr);

    // Additional member variables
    int SA_id;
    uint64_t sa_clk;
    int L_Q_pop_cnt;
    int R_Q_pop_cnt;

    std::vector<PimUnit*> pim_unit_; // Vector to store 2 PIM Units

    uint8_t* pmemAddr_;
    uint64_t pmemAddr_size_;
    unsigned int burstSize_;
    
    uint32_t *column_data;
    uint32_t column_index; 
    uint32_t previous_column;

    uint32_t accumulate_count;

protected:
    Config &config_;
};

}  // namespace dramsim3

#endif  // SHARED_ACC_H
