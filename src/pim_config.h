#ifndef __CONFIG_H_
#define __CONFIG_H_

#include "./half.hpp"
using half_float::half;

/////////////////- set unit size - ///////////////////
//단위를 여기서 지정할 수 있도록 제작해 놓음
//typedef half              unit_t; //half = FP16
typedef uint16_t          unit_t; //uint16_t = INT16 //TW added
#define debug_mode        
#define watch_pimindex    1
//////////////////////////////////////////////////////

#define NOP_END           111
#define EXIT_END          222
//TW added
#define TRIGGER_SACC      333
#define RETURN_GACC       444

// SIZE IS BYTE
#define UNIT_SIZE         (int)(sizeof(unit_t)) //unit_t = 2B
#define WORD_SIZE         32 //32B
#define UNITS_PER_WORD    (WORD_SIZE / UNIT_SIZE) //16

#define GRF_SIZE          (8 * UNITS_PER_WORD * UNIT_SIZE) //8 * 16 * 2 = 256B
#define SRF_SIZE          (8 * UNIT_SIZE) //8 * 2 = 16B

enum class PIM_OPERATION {
    NOP = 0,
    JUMP,
    EXIT,
    MOV = 4,
    FILL,
    ADD = 8,
    MUL,
    MAC,
    MAD,
    SACC //TW added #12 //SACC는 MOV와 동일 But CLK 문제로 인해 우선 따로 추가
};

enum class PIM_OP_TYPE {
    CONTROL = 0, 
    DATA, 
    ALU,
    SHARED //TW added #3 //SHARED는 SACC와 동일 But CLK 문제로 인해 우선 따로 추가
};

enum class PIM_OPERAND {
    BANK = 0,
    GRF_A,
    GRF_B,
    SRF_A,
    SRF_M,
    NONE
};

#endif  // __CONFIG_H_
