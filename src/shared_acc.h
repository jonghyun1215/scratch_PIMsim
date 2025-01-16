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
private:
    std::queue<Element> L_IQ;
    std::queue<Element> R_IQ;
    int accumulators[8];  // Assuming 8 accumulators

public:
    // Constructor
    SharedAccumulator(Config &config, int id, PimUnit& pim1, PimUnit& pim2);

    // Additional methods
    void loadIndices(std::vector<int> L_indices, std::vector<int> R_indices);
    void simulateStep();
    void loadUnit(int index);
    void runSimulation();
    void PrintClk();

    // Additional member variables
    int SA_id;
    int sa_clk;
    int L_Q_pop_cnt;
    int R_Q_pop_cnt;

    std::vector<PimUnit*> pim_unit_; // Vector to store 2 PIM Units

protected:
    Config &config_;
};

}  // namespace dramsim3

#endif  // SHARED_ACC_H
