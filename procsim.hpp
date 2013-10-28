#ifndef PROCSIM_HPP
#define PROCSIM_HPP

#include <cstdint>

#define DEFAULT_K0 1
#define DEFAULT_K1 2
#define DEFAULT_K2 3
#define DEFAULT_R 8
#define DEFAULT_M 2
#define DEFAULT_F 4

typedef struct _proc_inst_t
{
    uint32_t instruction_address;
    int32_t op_code;
    int32_t src_reg[2];
    int32_t dest_reg;
    
    // You may introduce other fields as needed
    
} proc_inst_t;

typedef struct _proc_stats_t
{
    float avg_inst_retired;
    unsigned long retired_instruction;
    unsigned long cycle_count;
} proc_stats_t;

bool read_instruction(proc_inst_t* p_inst);

void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m);
void run_proc(proc_stats_t* p_stats);
void complete_proc(proc_stats_t* p_stats);

#endif /* PROCSIM_HPP */
