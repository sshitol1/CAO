/*
 * apex_cpu.c
 * Contains APEX cpu pipeline implementation
 *
 * Author:
 * Copyright (c) 2020, Gaurav Kothari (gkothar1@binghamton.edu)
 * State University of New York at Binghamton
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apex_cpu.h"
#include "apex_macros.h"

/* Converts the PC(4000 series) into array index for code memory
 *
 * Note: You are not supposed to edit this function
 */
static int
get_code_memory_index_from_pc(const int pc)
{
    return (pc - 4000) / 4;
}

static void
print_instruction(const CPU_Stage *stage)
{
    switch (stage->opcode)
    {
        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_MUL:
        case OPCODE_DIV:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        {
            printf("%s,R%d,R%d,R%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->rs2);
            break;
        }

        case OPCODE_MOVC:
        {
            printf("%s,R%d,#%d ", stage->opcode_str, stage->rd, stage->imm);
            break;
        }

        case OPCODE_LOAD:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->imm);
            break;
        }

        case OPCODE_LOADP:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
                stage->imm);
            break;
        }


        case OPCODE_STORE:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rs1, stage->rs2,
                   stage->imm);
            break;
        }

        case OPCODE_STOREP:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rs1, stage->rs2,
                   stage->imm);
            break;
        }


        case OPCODE_BZ:
        case OPCODE_BNZ:
        case OPCODE_BP:
        case OPCODE_BNP:
        case OPCODE_BN:
        case OPCODE_BNN:
        {
            printf("%s,#%d ", stage->opcode_str, stage->imm);
            break;
        }

        case OPCODE_HALT:
        {
            printf("%s", stage->opcode_str);
            break;
        }

        case OPCODE_NOP:
        {
            printf("%s", stage->opcode_str);
            break;
        }

        case OPCODE_ADDL:
        case OPCODE_SUBL:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->imm);
            break;
        }

        case OPCODE_CMP:
        {
            printf("%s,R%d,R%d ", stage->opcode_str, stage->rs1, stage->rs2);
            break;
        }

        case OPCODE_CML:
        {
            printf("%s,R%d,#%d ", stage->opcode_str, stage->rs1, stage->imm);
            break;
        }

        case OPCODE_JUMP:
        {
            printf("%s,R%d,#%d ", stage->opcode_str, stage->rs1, stage->imm);
            break;
        }

        case OPCODE_JALR:
        {
            printf("%s,R%d,R%d,#%d ", stage->opcode_str, stage->rd, stage->rs1,
                   stage->imm);
            break;
        }

    }
}

/* Debug function which prints the CPU stage content
 *
 * Note: You can edit this function to print in more detail
 */
static void
print_stage_content(const char *name, const CPU_Stage *stage)
{
    printf("%-15s: pc(%d) ", name, stage->pc);
    print_instruction(stage);
    printf("\n");
}

/* Debug function which prints the register file
 *
 * Note: You are not supposed to edit this function
 */
static void
print_reg_file(const APEX_CPU *cpu)
{
    int i;

    printf("----------\n%s\n----------\n", "Registers:");

    for (int i = 0; i < REG_FILE_SIZE / 2; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");

    for (i = (REG_FILE_SIZE / 2); i < REG_FILE_SIZE; ++i)
    {
        printf("R%-3d[%-3d] ", i, cpu->regs[i]);
    }

    printf("\n");
}

void initialize_BTB() {
    for (int i = 0; i < BTB_SIZE; ++i) {
        btb[i].instruction_address = -1; // Indicates an empty entry
        btb[i].history_bits = 0; // Initialize based on the branch type
        btb[i].target_address = -1;
    }
}

int is_write_to_reg_instruction(int opcode)
{
    switch (opcode)
    {
        case OPCODE_ADD:
        case OPCODE_SUB:
        case OPCODE_MUL:
        case OPCODE_AND:
        case OPCODE_OR:
        case OPCODE_XOR:
        case OPCODE_MOVC:
        case OPCODE_ADDL:
        case OPCODE_SUBL:
            return 1;
        default:
            return 0;
    }
}

int should_take_branch(int history_bits, int branch_type) {
    // Check branch type and make prediction based on history bits
    switch (branch_type) {
        case OPCODE_BNZ: // Fall through
        case OPCODE_BP:
            // Predict taken if any of the last two were taken (i.e., if any bit is 1)
            return (history_bits & 0b01) || (history_bits & 0b10);

        case OPCODE_BZ: // Fall through
        case OPCODE_BNP:
            // Predict not taken if any of the last two were not taken (i.e., if any bit is 0)
            return !((history_bits & 0b01) && (history_bits & 0b10));

        default:
            // For other types of branches or instructions, handle accordingly
            // Or return a default prediction
            return 0; // Or any default value
    }
}


/*
 * Fetch Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_fetch(APEX_CPU *cpu)
{
    if (cpu->fetch.stall) {
        // Skip fetching new instruction
        // printf("Fetch stage stall flag before fetch: %d\n", cpu->fetch.stall);
        // printf("Decode stage stall flag before fetch: %d\n", cpu->decode.stall);
        detect_data_hazards(cpu);

        return;
    }
    APEX_Instruction *current_ins;

    if (cpu->fetch.opcode == OPCODE_BZ ||
        cpu->fetch.opcode == OPCODE_BNZ ||
        cpu->fetch.opcode == OPCODE_BP ||
        cpu->fetch.opcode == OPCODE_BNP) {
        int btb_index = find_in_BTB(cpu->fetch.pc);

    if (btb_index != -1) {
        // Get the opcode (branch type) of the current instruction
        int branch_type = cpu->fetch.opcode;

        // Check if the branch should be taken based on the history bits and branch type
        if (should_take_branch(btb[btb_index].history_bits, branch_type)) {
            // Branch is predicted to be taken
            cpu->pc = btb[btb_index].target_address;
        } else {
            // Branch is predicted not to be taken
            cpu->pc += 4; // Move to the next instruction
        }
    }
        }

    if (cpu->fetch.has_insn)
    {
        /* This fetches new branch target instruction from next cycle */
        if (cpu->fetch_from_next_cycle == TRUE)
        {
            cpu->fetch_from_next_cycle = FALSE;

            /* Skip this cycle*/
            return;
        }


        /* Store current PC in fetch latch */
        cpu->fetch.pc = cpu->pc;

        /* Index into code memory using this pc and copy all instruction fields
         * into fetch latch  */
        current_ins = &cpu->code_memory[get_code_memory_index_from_pc(cpu->pc)];
        strcpy(cpu->fetch.opcode_str, current_ins->opcode_str);
        cpu->fetch.opcode = current_ins->opcode;
        cpu->fetch.rd = current_ins->rd;
        cpu->fetch.rs1 = current_ins->rs1;
        cpu->fetch.rs2 = current_ins->rs2;
        cpu->fetch.imm = current_ins->imm;

        /* Update PC for next instruction */
        cpu->pc += 4;

        /* Copy data from fetch latch to decode latch*/
        cpu->decode = cpu->fetch;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Fetch", &cpu->fetch);
        }

        /* Stop fetching new instructions if HALT is fetched */
        if (cpu->fetch.opcode == OPCODE_HALT)
        {
            cpu->fetch.has_insn = FALSE;
        }

        if (cpu->fetch.opcode == OPCODE_NOP)
        {
            cpu->decode.has_insn = FALSE;
        }
    }
}

/*
 * Decode Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_decode(APEX_CPU *cpu)
{
    if (cpu->decode.stall) {
        // Skip fetching new instruction
        return;
    }
    if (cpu->decode.has_insn)
    {
        if (is_write_to_reg_instruction(cpu->execute.opcode) && 
            (cpu->decode.rs1 == cpu->execute.rd || cpu->decode.rs2 == cpu->execute.rd)) {
            // Forward data from Execute stage
            if (cpu->decode.rs1 == cpu->execute.rd) {
                cpu->decode.rs1_value = cpu->execute.result_buffer;
            }
            if (cpu->decode.rs2 == cpu->execute.rd) {
                cpu->decode.rs2_value = cpu->execute.result_buffer;
            }
        }

        /* Check for data forwarding from Memory stage */
        if (is_write_to_reg_instruction(cpu->memory.opcode) && 
            (cpu->decode.rs1 == cpu->memory.rd || cpu->decode.rs2 == cpu->memory.rd)) {
            // Forward data from Memory stage
            if (cpu->decode.rs1 == cpu->memory.rd) {
                cpu->decode.rs1_value = cpu->memory.result_buffer;
            }
            if (cpu->decode.rs2 == cpu->memory.rd) {
                cpu->decode.rs2_value = cpu->memory.result_buffer;
            }
        }
        /* Read operands from register file based on the instruction type */
        switch (cpu->decode.opcode)
        {
            case OPCODE_ADD:
            case OPCODE_ADDL:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                break;
            }

            case OPCODE_SUB:
            case OPCODE_SUBL:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                break;
            }

            case OPCODE_OR:
            case OPCODE_XOR:
            case OPCODE_AND:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                break;
            }

            case OPCODE_MUL:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                break;
            }

            case OPCODE_LOAD:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                printf("hi");
                // cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                break;
            }

            case OPCODE_STORE:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                printf("%d",cpu->decode.rs1_value);
                printf("%d",cpu->decode.rs2_value);
                break;
            }

            case OPCODE_STOREP:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                // printf("%d",cpu->decode.rs1_value);
                // printf("%d",cpu->decode.rs2_value);
                break;
            }

            case OPCODE_LOADP:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rd_value = cpu->regs[cpu->decode.rd];
                break;
            }

            case OPCODE_MOVC:
            {
                /* MOVC doesn't have register operands */
                break;
            }

            case OPCODE_CMP:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rs2_value = cpu->regs[cpu->decode.rs2];
                break;
            }

            case OPCODE_CML:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                break;
            }

            case OPCODE_JUMP:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                break;
            }

            case OPCODE_JALR:
            {
                cpu->decode.rs1_value = cpu->regs[cpu->decode.rs1];
                cpu->decode.rd_value = cpu->regs[cpu->decode.rd];
                break;
            }
        }

        /* Copy data from decode latch to execute latch*/
        cpu->execute = cpu->decode;
        cpu->decode.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Decode/RF", &cpu->decode);
        }
    }
}

/*
 * Execute Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_execute(APEX_CPU *cpu)
{
    if (cpu->execute.stall) {
        // Skip fetching new instruction
        return;
    }
    if (cpu->execute.has_insn)
    {
        /* Execute logic based on instruction type */
        switch (cpu->execute.opcode)
        {
            case OPCODE_ADD:
            {
                cpu->execute.result_buffer
                    = cpu->execute.rs1_value + cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                if (cpu->execute.result_buffer == 0)
                {
                    cpu->zero_flag = TRUE;
                } 
                else 
                {
                    cpu->zero_flag = FALSE;
                }
                break;
            }

            case OPCODE_ADDL:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.imm;
                /* Set the zero flag based on the result buffer */
                if (cpu->execute.result_buffer == 0)
                {
                    cpu->zero_flag = TRUE;
                } 
                else 
                {
                    cpu->zero_flag = FALSE;
                }
                break;
            }
            
            case OPCODE_SUB:
            {
                cpu->execute.result_buffer
                    = cpu->execute.rs1_value - cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                if (cpu->execute.result_buffer == 0)
                {
                    cpu->zero_flag = TRUE;
                } 
                else 
                {
                    cpu->zero_flag = FALSE;
                }
                break;
            }

            case OPCODE_SUBL:
            {
                cpu->execute.result_buffer
                    = cpu->execute.rs1_value - cpu->execute.imm;
                    
                /* Set the zero flag based on the result buffer */
                if (cpu->execute.result_buffer == 0)
                {
                    cpu->zero_flag = TRUE;
                } 
                else 
                {
                    cpu->zero_flag = FALSE;
                }
                break;
            }

            case OPCODE_MUL:
            {
                cpu->execute.result_buffer
                    = cpu->execute.rs1_value - cpu->execute.rs2_value;
                break;
            }

            case OPCODE_LOAD:
            {
                cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
                printf("%d",cpu->execute.memory_address);
                break;
            }

            case OPCODE_LOADP:
            {
                /* Calculate the memory address by adding rs1_value and rs2_value */
                cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
                cpu->execute.rd = cpu->execute.rd_value + 4 ;
                break;
            }
            
            case OPCODE_STORE:
            {
                cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
                cpu->execute.result_buffer = cpu->execute.rs2_value;
                printf("%d",cpu->execute.result_buffer);
                printf("%d",cpu->execute.memory_address);
                break;
            }

            case OPCODE_STOREP:
            {
                cpu->execute.memory_address = cpu->execute.rs1_value + cpu->execute.imm;
                cpu->execute.result_buffer = cpu->execute.rs2_value;
                // printf("%d",cpu->execute.result_buffer);
                // printf("%d",cpu->execute.memory_address);
                break;
            }

            case OPCODE_JUMP:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.imm;

                cpu->pc = cpu->execute.result_buffer;

                cpu->fetch_from_next_cycle = TRUE;

                cpu->decode.has_insn = FALSE;

                cpu->fetch.has_insn = TRUE;

                break;
            }

            case OPCODE_JALR: 
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value + cpu->execute.imm;

                cpu->pc = cpu->execute.result_buffer;

                cpu->regs[cpu->execute.rd_value] = cpu->execute.pc + 4;

                cpu->fetch_from_next_cycle = TRUE;

                cpu->decode.has_insn = FALSE;

                cpu->fetch.has_insn = TRUE;

                break;
            }


            case OPCODE_BZ:
            {
                if (cpu->zero_flag == TRUE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    
                    /* Since we are using reverse callbacks for pipeline stages, 
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_BNZ:
            {
                if (cpu->zero_flag == FALSE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    
                    /* Since we are using reverse callbacks for pipeline stages, 
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_BP:
            {
                if (cpu->pos_flag == TRUE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    
                    /* Since we are using reverse callbacks for pipeline stages, 
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_BNP:
            {
                if (cpu->pos_flag == FALSE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    
                    /* Since we are using reverse callbacks for pipeline stages, 
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_BN:
            {
                if (cpu->neg_flag == TRUE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    
                    /* Since we are using reverse callbacks for pipeline stages, 
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_BNN:
            {
                if (cpu->neg_flag == FALSE)
                {
                    /* Calculate new PC, and send it to fetch unit */
                    cpu->pc = cpu->execute.pc + cpu->execute.imm;
                    
                    /* Since we are using reverse callbacks for pipeline stages, 
                     * this will prevent the new instruction from being fetched in the current cycle*/
                    cpu->fetch_from_next_cycle = TRUE;

                    /* Flush previous stages */
                    cpu->decode.has_insn = FALSE;

                    /* Make sure fetch stage is enabled to start fetching from new PC */
                    cpu->fetch.has_insn = TRUE;
                }
                break;
            }

            case OPCODE_CMP:
            {
                if (cpu->execute.rs1_value == cpu->execute.rs2_value)
                {
                    cpu->zero_flag = TRUE;
                    cpu->neg_flag = FALSE;
                    cpu->pos_flag = FALSE;
                }

                else if (cpu->execute.rs1_value < cpu->execute.rs2_value)
                {
                    cpu->zero_flag = FALSE;
                    cpu->neg_flag = TRUE;
                    cpu->pos_flag = FALSE;
                }

                else
                {
                    cpu->zero_flag = FALSE;
                    cpu->neg_flag = FALSE;
                    cpu->pos_flag = TRUE;
                }
            }

            case OPCODE_CML:
            {
                if (cpu->execute.rs1_value == cpu->execute.imm)
                {
                    cpu->zero_flag = TRUE;
                    cpu->neg_flag = FALSE;
                    cpu->pos_flag = FALSE;
                }

                else if (cpu->execute.rs1_value < cpu->execute.imm)
                {
                    cpu->zero_flag = FALSE;
                    cpu->neg_flag = TRUE;
                    cpu->pos_flag = FALSE;
                }

                else
                {
                    cpu->zero_flag = FALSE;
                    cpu->neg_flag = FALSE;
                    cpu->pos_flag = TRUE;
                }
            }

            case OPCODE_MOVC: 
            {
                cpu->execute.result_buffer = cpu->execute.imm;

                /* Set the zero flag based on the result buffer */
                if (cpu->execute.result_buffer == 0)
                {
                    cpu->zero_flag = TRUE;
                } 
                else 
                {
                    cpu->zero_flag = FALSE;
                }
                break;
            }

            case OPCODE_OR:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value | cpu->execute.rs2_value;
                break;
            }

            case OPCODE_XOR:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value ^ cpu->execute.rs2_value;
                break;
            }

            case OPCODE_AND:
            {
                cpu->execute.result_buffer = cpu->execute.rs1_value & cpu->execute.rs2_value;

                /* Set the zero flag based on the result buffer */
                if (cpu->execute.result_buffer == 0)
                {
                    cpu->zero_flag = TRUE;
                } 
                else 
                {
                    cpu->zero_flag = FALSE;
                }
                break;
            }
        }

        /* Copy data from execute latch to memory latch*/
        cpu->memory = cpu->execute;
        cpu->execute.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Execute", &cpu->execute);
        }
    }
}

/*
 * Memory Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static void
APEX_memory(APEX_CPU *cpu)
{
    if (cpu->memory.stall) {
        // Skip fetching new instruction
        return;
    }
    if (cpu->memory.has_insn)
    {
        switch (cpu->memory.opcode)
        {
            case OPCODE_ADD:
            {
                /* No work for ADD */
                break;
            }

            case OPCODE_LOAD:
            {
                /* Read from data memory */
                cpu->memory.result_buffer
                    = cpu->data_memory[cpu->memory.memory_address];
                printf("%d",cpu->memory.result_buffer);
                break;
            }
            case OPCODE_LOADP:
            {
                /* Read from data memory */
                cpu->memory.result_buffer
                    = cpu->data_memory[cpu->memory.memory_address];
                // printf("%d",cpu->memory.result_buffer);
                break;
            }

            case OPCODE_STORE:
            {
                /* Write to data memory */
                cpu->data_memory[cpu->memory.memory_address] = cpu->memory.result_buffer;
                break;
            }

            case OPCODE_STOREP:
            {
                int data_to_store = cpu->memory.result_buffer;

                cpu->data_memory[cpu->memory.memory_address] = data_to_store;
                break;
            }


        }

        /* Copy data from memory latch to writeback latch*/
        cpu->writeback = cpu->memory;
        cpu->memory.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Memory", &cpu->memory);
        }
    }
}

/*
 * Writeback Stage of APEX Pipeline
 *
 * Note: You are free to edit this function according to your implementation
 */
static int
APEX_writeback(APEX_CPU *cpu)
{
    if (cpu->writeback.stall) {
        // Skip fetching new instruction
        return FALSE;
    }
    if (cpu->writeback.has_insn)
    {
        /* Write result to register file based on instruction type */
        switch (cpu->writeback.opcode)
        {
            case OPCODE_ADD:
            case OPCODE_ADDL:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_SUB:
            case OPCODE_SUBL:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_MUL:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_LOAD:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_LOADP:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_MOVC: 
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_OR:
            case OPCODE_XOR:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_JALR:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }

            case OPCODE_AND:
            {
                cpu->regs[cpu->writeback.rd] = cpu->writeback.result_buffer;
                break;
            }
        }

        cpu->insn_completed++;
        cpu->writeback.has_insn = FALSE;

        if (ENABLE_DEBUG_MESSAGES)
        {
            print_stage_content("Writeback", &cpu->writeback);
        }

        if (cpu->writeback.opcode == OPCODE_HALT)
        {
            /* Stop the APEX simulator */
            return TRUE;
        }
    }

    /* Default */
    return 0;
}

/*
 * This function creates and initializes APEX cpu.
 *
 * Note: You are free to edit this function according to your implementation
 */
APEX_CPU *
APEX_cpu_init(const char *filename)
{
    int i;
    APEX_CPU *cpu;

    if (!filename)
    {
        return NULL;
    }

    cpu = calloc(1, sizeof(APEX_CPU));

    if (!cpu)
    {
        return NULL;
    }

    cpu->fetch.stall = 0;
    cpu->decode.stall = 0;
    cpu->execute.stall = 0;
    cpu->memory.stall = 0;
    cpu->writeback.stall = 0;

    printf("Fetch stage stall flag before fetch: %d\n", cpu->fetch.stall);

    /* Initialize PC, Registers and all pipeline stages */
    cpu->pc = 4000;
    memset(cpu->regs, 0, sizeof(int) * REG_FILE_SIZE);
    memset(cpu->data_memory, 0, sizeof(int) * DATA_MEMORY_SIZE);
    cpu->single_step = ENABLE_SINGLE_STEP;

    /* Parse input file and create code memory */
    cpu->code_memory = create_code_memory(filename, &cpu->code_memory_size);
    if (!cpu->code_memory)
    {
        free(cpu);
        return NULL;
    }

    if (ENABLE_DEBUG_MESSAGES)
    {
        fprintf(stderr,
                "APEX_CPU: Initialized APEX CPU, loaded %d instructions\n",
                cpu->code_memory_size);
        fprintf(stderr, "APEX_CPU: PC initialized to %d\n", cpu->pc);
        fprintf(stderr, "APEX_CPU: Printing Code Memory\n");
        printf("%-9s %-9s %-9s %-9s %-9s\n", "opcode_str", "rd", "rs1", "rs2",
               "imm");

        for (i = 0; i < cpu->code_memory_size; ++i)
        {
            printf("%-9s %-9d %-9d %-9d %-9d\n", cpu->code_memory[i].opcode_str,
                   cpu->code_memory[i].rd, cpu->code_memory[i].rs1,
                   cpu->code_memory[i].rs2, cpu->code_memory[i].imm);
        }
    }

    /* To start fetch stage */
    cpu->fetch.has_insn = TRUE;
    return cpu;
}



void detect_data_hazards(APEX_CPU *cpu) {
    // Check if both stages have valid instructions
    if (cpu->execute.has_insn && cpu->decode.has_insn) {

        // Check if the execute stage instruction writes to a register
        if (cpu->execute.opcode == OPCODE_ADD  ||
            cpu->execute.opcode == OPCODE_SUB  ||
            cpu->execute.opcode == OPCODE_MUL  ||
            cpu->execute.opcode == OPCODE_AND  ||
            cpu->execute.opcode == OPCODE_OR   ||
            cpu->execute.opcode == OPCODE_XOR  ||
            cpu->execute.opcode == OPCODE_MOVC ||
            cpu->execute.opcode == OPCODE_ADDL ||
            cpu->execute.opcode == OPCODE_SUBL ) 
        {
            int execute_dest_reg = cpu->execute.rd;
            printf("rs1 %d\n", cpu->decode.rs1);
            printf("rs2 %d\n", cpu->decode.rs2);
            printf("rd %d\n", cpu->decode.rd);

            // Check if the execute stage instruction's destination register is non-zero
            // and if the decode stage instruction uses the same register as a source
            if (execute_dest_reg != 0 &&
                (cpu->decode.rs1 == execute_dest_reg || cpu->decode.rs2 == execute_dest_reg)) {
                // RAW hazard detected, stall the pipeline
                cpu->decode.stall = 1;
                cpu->fetch.stall = 1;
            } else {
                // No hazard, clear stall flags
                cpu->decode.stall = 0;
                cpu->fetch.stall = 0;
                APEX_fetch(cpu);
            }
        } else {
            // No hazard, clear stall flags
            cpu->decode.stall = 0;
            cpu->fetch.stall = 0;
        }
    }
}



/*
 * APEX CPU simulation loop
 *
 * Note: You are free to edit this function according to your implementation
 */
void
APEX_cpu_run(APEX_CPU *cpu)
{
    char user_prompt_val;

    while (TRUE)
    {


       
        if (ENABLE_DEBUG_MESSAGES)
        {
            printf("--------------------------------------------\n");
            printf("Clock Cycle #: %d\n", cpu->clock);
            printf("--------------------------------------------\n");
        }

        if (APEX_writeback(cpu))
        {
            /* Halt in writeback stage */
            printf("APEX_CPU: Simulation Complete, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
            break;
        }

        // APEX_memory(cpu);
        // APEX_execute(cpu);
        // APEX_decode(cpu);
        // APEX_fetch(cpu);

        if (!cpu->memory.stall) {
        APEX_memory(cpu);
        }else {
            // Check if the stall condition is resolved
            detect_data_hazards(cpu);
        }

        // Execute Stage
        if (!cpu->execute.stall) {
            APEX_execute(cpu);
        }else {
            // Check if the stall condition is resolved
            detect_data_hazards(cpu);
        }

        // Decode Stage
        if (!cpu->decode.stall) {
            APEX_decode(cpu);
        } else {
            // Check if the stall condition is resolved
            detect_data_hazards(cpu);
        }

        // Fetch Stage
        if (!cpu->fetch.stall) {
            APEX_fetch(cpu);
        }else {
            // Check if the stall condition is resolved
            detect_data_hazards(cpu);
        }

        // Detect hazards before running the stages
        detect_data_hazards(cpu);

        print_reg_file(cpu);

        if (cpu->single_step)
        {
            printf("Press any key to advance CPU Clock or <q> to quit:\n");
            scanf("%c", &user_prompt_val);

            if ((user_prompt_val == 'Q') || (user_prompt_val == 'q'))
            {
                printf("APEX_CPU: Simulation Stopped, cycles = %d instructions = %d\n", cpu->clock, cpu->insn_completed);
                break;
            }
        }

        cpu->clock++;
    }
}

/*
 * This function deallocates APEX CPU.
 *
 * Note: You are free to edit this function according to your implementation
 */
void
APEX_cpu_stop(APEX_CPU *cpu)
{
    free(cpu->code_memory);
    free(cpu);
}