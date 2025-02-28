#ifndef __PIM_FUNC_SIM_H
#define __PIM_FUNC_SIM_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "pim_unit.h"
#include "configuration.h"
#include "common.h"
//TW added
#include "shared_acc.h"
#include "global_acc.h"


namespace dramsim3 {

class PimFuncSim {
 public:
    PimFuncSim(Config &config);
    void AddTransaction(Transaction *trans);
    bool DebugMode(uint64_t hex_addr);
    bool ModeChanger(uint64_t hex_addr);

    std::vector<string> bankmode;
    std::vector<bool> PIM_OP_MODE; //16개 channel에 대한 PIM mode 여부
    std::vector<PimUnit*> pim_unit_;
    //TW added
    std::vector<SharedAccumulator*> shared_acc_;
    std::vector<GlobalAccumulator*> global_acc_;
    //added end

    uint8_t* pmemAddr;
    uint64_t pmemAddr_size;
    unsigned int burstSize;

    uint64_t ReverseAddressMapping(Address& addr);
    uint64_t GetPimIndex(Address& addr);
    void PmemWrite(uint64_t hex_addr, uint8_t* DataPtr);
    void PmemRead(uint64_t hex_addr, uint8_t* DataPtr);
    void init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
              unsigned int burstSize);

   //TW added
   //To print value how many accumulated
   int accumulation_count = 0;

 protected:
    Config &config_;
};

}  // namespace dramsim3

#endif  // __PIM_FUNC_SIM_H
