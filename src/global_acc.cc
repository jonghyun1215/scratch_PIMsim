#include "./global_acc.h"
#include <iostream>

namespace dramsim3{

void GlobalAccumulator::init(uint8_t* pmemAddr, uint64_t pmemAddr_size,
                   unsigned int burstSize) {
    pmemAddr_ = pmemAddr;
    pmemAddr_size_ = pmemAddr_size;
    burstSize_ = burstSize;
}

// void 로 해도 상관 없을 듯
// Bank 데이터를 한번에 읽어와야 되는 상황을 대비해 일단 만들어 놓음 BUT 
// Channel level parallelism을 이용하는 경우라면 굳이...
int GlobalAccumulator::AddTransaction(uint64_t hex_addr, bool is_write, uint8_t* DataPtr) {
    // Add transaction to the memory system
    // memory_system_.AddTransaction(hex_addr, is_write, DataPtr);
    if(!is_write)
        memcpy(bank_data_ , pmemAddr_ + hex_addr, WORD_SIZE); 
    
    return 0; 
}

GlobalAccumulator::GlobalAccumulator(Config &config)
    :config_(config)
{
    // Initialize the queues
    for (int i = 0; i < 16; i++) {
        pair_queue_1[i] = std::queue<Pair>();
    }
    for (int i = 0; i < 8; i++) {
        pair_queue_2[i] = std::queue<Pair>();
    }
    for (int i = 0; i < 4; i++) {
        pair_queue_3[i] = std::queue<Pair>();
    }
    for (int i = 0; i < 2; i++) {
        pair_queue_4[i] = std::queue<Pair>();
    }
    result_pair_queue = std::queue<Pair>();

    // Initialize the accumulators
}

int GlobalAccumulator::StartAcc() {
    #ifdef debug_mode
    std::cout << "Triggering global accumulator\n";
    #endif
    while (!IsAllQueueEmpty()) {
        simulate_step();
    }
    return 0; // 작업이 완료되면 0 반환
}

int GlobalAccumulator::IsAllQueueEmpty() {
    for (auto& q : pair_queue_1) {
        if (!q.empty()) return 0;
    }
    for (auto& q : pair_queue_2) {
        if (!q.empty()) return 0;
    }
    for (auto& q : pair_queue_3) {
        if (!q.empty()) return 0;
    }
    for (auto& q : pair_queue_4) {
        if (!q.empty()) return 0;
    }
    return result_pair_queue.empty() ? 1 : 0;
}

//외부에서 들어온 데이터를 queue에 추가하는 함수
void GlobalAccumulator::AddDataToPairQueue1(int queue_index, const Pair& data) {
    if (queue_index >= 0 && queue_index < 16) {
        pair_queue_1[queue_index].push(data);
    } else {
        // 유효하지 않은 인덱스에 대해 에러 처리 (예: 로그 메시지 출력)
        std::cerr << "Invalid queue index: " << queue_index << std::endl;
    }
}

//Processing logic in the
//Pair = Index + Data
Pair GlobalAccumulator::compare_and_add(Pair LQ_front, Pair RQ_front, bool &should_add) {
    if (LQ_front.index == RQ_front.index) {
        should_add = true;
        Pair result;
        result.index = LQ_front.index;
        result.data = LQ_front.data + RQ_front.data; // data 값 더하기
        return result;
    } else if (LQ_front.index > RQ_front.index) {
        should_add = false;
        return RQ_front; // RQ만 pop
    } else {
        should_add = false;
        return LQ_front; // LQ만 pop
    }
}

void GlobalAccumulator::simulate_step() {
    for (int i = 0; i < 16; i += 2) {
        process_queues(pair_queue_1[i], pair_queue_1[i + 1], pair_queue_2[i / 2]);
    }
    for (int i = 0; i < 8; i += 2) {
        process_queues(pair_queue_2[i], pair_queue_2[i + 1], pair_queue_3[i / 2]);
    }
    for (int i = 0; i < 4; i += 2) {
        process_queues(pair_queue_3[i], pair_queue_3[i + 1], pair_queue_4[i / 2]);
    }
    process_queues(pair_queue_4[0], pair_queue_4[1], result_pair_queue);
}

void GlobalAccumulator::process_queues(std::queue<Pair>& LQ, std::queue<Pair>& RQ, std::queue<Pair>& next_queue) {
    if (!LQ.empty() && !RQ.empty()) {
        //Pair = Index + Data
        Pair LQ_front = LQ.front();
        Pair RQ_front = RQ.front();

        bool should_add = false;
        Pair result = compare_and_add(LQ_front, RQ_front, should_add);

        if (should_add) {
            next_queue.push(result); // 더한 결과를 큐에 추가
            LQ.pop();
            RQ.pop();
        } else if (LQ_front.index == result.index) {
            next_queue.push(result); // 그냥 pop 된 결과를 queue에 추가
            RQ.pop();
        } else {
            next_queue.push(result); // 그냥 pop 된 결과를 queue에 추가
            LQ.pop();
        }
    }
}

}