/*
 * apex_cpu.h
 * Contains APEX cpu pipeline declarations
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#ifndef _APEX_CPU_H_
#define _APEX_CPU_H_

#include "apex_macros.h"



/* Format of an APEX instruction  */
typedef struct APEX_Instruction
{
    char opcode_str[128];
    int opcode;
    int rd;
    int rs1;
    int rs2;
    int imm;
    int is_lit;    //flag to identify if the operand is literal or not
    int is_store;  // flag to show store command
    int stall;
} APEX_Instruction;

/* Model of CPU stage latch */
typedef struct CPU_Stage
{
    int pc;
    char opcode_str[128];
    int opcode;
    int rs1;
    int rs2;
    int rd;
    int imm;
    int is_lit;
    int rs1_value;
    int rs2_value;
    int rd_value;
    int result_buffer;
    int memory_address;
    int has_insn;
    int data_of_store;
    int stall;
    int hazard_pres;
} CPU_Stage;

typedef struct APEX_Reg_Status 
{
    int value;
    int status; // 0 means available, 1 means being used
    int valid;
} APEX_Reg_Status;

typedef struct BTB_Entry {
    int instruction_address;
    int history_bits; // Use two bits to record the outcome of the last two executions
    int target_address;
    // Additional fields if necessary (e.g., for identifying the victim entry)
} BTB_Entry;


/* Model of APEX CPU */
typedef struct APEX_CPU
{
    int pc;                        /* Current program counter */
    int clock;                     /* Clock cycles elapsed */
    int insn_completed;            /* Instructions retired */
    int regs[REG_FILE_SIZE];       /* Integer register file */
    int code_memory_size;          /* Number of instruction in the input file */
    APEX_Reg_Status register_status[REG_FILE_SIZE]; // Status of registers
    APEX_Instruction *code_memory; /* Code Memory */
    int data_memory[DATA_MEMORY_SIZE]; /* Data Memory */
    int single_step;               /* Wait for user input after every cycle */
    int zero_flag;                 /* {TRUE, FALSE} Used by BZ and BNZ to branch */
    int pos_flag;                  
    int neg_flag;
    int cc;                        
    int fetch_from_next_cycle;

    /* Pipeline stages */
    CPU_Stage fetch;
    CPU_Stage decode;
    CPU_Stage execute;
    CPU_Stage memory;
    CPU_Stage writeback;
} APEX_CPU;

APEX_Instruction *create_code_memory(const char *filename, int *size);
APEX_CPU *APEX_cpu_init(const char *filename);
void APEX_cpu_run(APEX_CPU *cpu);
void APEX_cpu_stop(APEX_CPU *cpu);
void detect_data_hazards(APEX_CPU *cpu);
#endif
