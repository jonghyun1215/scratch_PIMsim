#ifndef __GLOBAL_ACC_H_
#define __GLOBAL_ACC_H_

#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cmath>
#include "./pim_config.h"
#include "./pim_utils.h"
#include "./configuration.h"
#include "./common.h"
#include "./half.hpp"
#include <queue> 

namespace dramsim3 {

typedef struct Pair{
    uint32_t index;
    unit_t data; 
}Pair;

class GlobalAccumulator {
public:
    GlobalAccumulator(Config &config); //GA는 모든 채널ㅇ르 바라보고 있기에 ID가 필요 없음
    //Read CMD를 통해 데이터를 Global accumulator가 읽어올 수 있도록
    int AddTransaction(uint64_t hex_addr, bool is_write, uint8_t* DataPtr);

    int StartAcc();
    int IsAllQueueEmpty();
    
    // 외부에서 pair_queue_1에 데이터를 추가하는 함수
    void AddDataToPairQueue1(int queue_index, const Pair& data);
    std::queue<Pair> pair_queue_1[16];
    std::queue<Pair> pair_queue_2[8];
    std::queue<Pair> pair_queue_3[4];
    std::queue<Pair> pair_queue_4[2];
    std::queue<Pair> result_pair_queue;
    
    void init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
              unsigned int burstSize);
        
    Pair compare_and_add(Pair LQ_front, Pair RQ_front, bool &should_add);
    void simulate_step();
    void process_queues(std::queue<Pair>& LQ, std::queue<Pair>& RQ, std::queue<Pair>& result_queue);

    unit_t *bank_data_;
    uint8_t* pmemAddr_;
    uint64_t pmemAddr_size_;
    unsigned int burstSize_;


protected:
    Config &config_;

};

} // namespace dramsim3

#endif  // __GLOBAL_ACC_H_