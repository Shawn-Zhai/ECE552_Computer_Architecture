#include <limits.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "sim.h"
#include "decode.def"

#include "instr.h"

/* PARAMETERS OF THE TOMASULO'S ALGORITHM */

/* ECE552 Assignment 3 - BEGIN CODE */
#define INSTR_QUEUE_SIZE         16 //10

#define RESERV_INT_SIZE    5 //4
#define RESERV_FP_SIZE     3 //2
#define FU_INT_SIZE        3 //2
#define FU_FP_SIZE         1

#define FU_INT_LATENCY     5 //4
#define FU_FP_LATENCY      7 //9
/* ECE552 Assignment 3 - END CODE */

/* IDENTIFYING INSTRUCTIONS */

//unconditional branch, jump or call
#define IS_UNCOND_CTRL(op) (MD_OP_FLAGS(op) & F_CALL || \
                         MD_OP_FLAGS(op) & F_UNCOND)

//conditional branch instruction
#define IS_COND_CTRL(op) (MD_OP_FLAGS(op) & F_COND)

//floating-point computation
#define IS_FCOMP(op) (MD_OP_FLAGS(op) & F_FCOMP)

//integer computation
#define IS_ICOMP(op) (MD_OP_FLAGS(op) & F_ICOMP)

//load instruction
#define IS_LOAD(op)  (MD_OP_FLAGS(op) & F_LOAD)

//store instruction
#define IS_STORE(op) (MD_OP_FLAGS(op) & F_STORE)

//trap instruction
#define IS_TRAP(op) (MD_OP_FLAGS(op) & F_TRAP) 

#define USES_INT_FU(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_STORE(op))
#define USES_FP_FU(op) (IS_FCOMP(op))

#define WRITES_CDB(op) (IS_ICOMP(op) || IS_LOAD(op) || IS_FCOMP(op))

/* FOR DEBUGGING */

//prints info about an instruction
#define PRINT_INST(out,instr,str,cycle) \
  myfprintf(out, "%d: %s", cycle, str); \
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

#define PRINT_REG(out,reg,str,instr) \
  myfprintf(out, "reg#%d %s ", reg, str); \
  md_print_insn(instr->inst, instr->pc, out); \
  myfprintf(stdout, "(%d)\n",instr->index);

/* VARIABLES */

//instruction queue for Tomasulo
static instruction_t* instr_queue[INSTR_QUEUE_SIZE];
//number of instructions in the instruction queue
static int instr_queue_size = 0;

//reservation stations (each reservation station entry contains a pointer to an instruction)
static instruction_t* reservINT[RESERV_INT_SIZE];
static instruction_t* reservFP[RESERV_FP_SIZE];

//functional units
static instruction_t* fuINT[FU_INT_SIZE];
static instruction_t* fuFP[FU_FP_SIZE];

//common data bus
static instruction_t* commonDataBus = NULL;

//The map table keeps track of which instruction produces the value for each register
static instruction_t* map_table[MD_TOTAL_REGS];

//the index of the instruction fetched
static int fetch_index = 1;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */

/* ECE552 Assignment 3 - BEGIN CODE */
int find_available_index(instruction_t** arr, int arr_size){
  for(int i=0; i<arr_size; i++){
    if(arr[i] == NULL){
      return i;
    }
  }
  return -1;
}

void insert_into_ordered_array(int* cycle_array, instruction_t** instr_array, int size) {
    int i = size - 1;
    int cycle = cycle_array[i];
    instruction_t* instr = instr_array[i];
    i--;
    while (i >= 0 && cycle_array[i] > cycle) {
        cycle_array[i+1] = cycle_array[i];
        instr_array[i+1] = instr_array[i];
        i--;
    }
    cycle_array[i + 1] = cycle;
    instr_array[i+1] = instr;
}
/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Checks if simulation is done by finishing the very last instruction
 *      Remember that simulation is done only if the entire pipeline is empty
 * Inputs:
 * 	sim_insn: the total number of instructions simulated
 * Returns:
 * 	True: if simulation is finished
 */
static bool is_simulation_done(counter_t sim_insn) {
  
  /* ECE552 Assignment 3 - BEGIN CODE */

  // done only if all instructions are finished processing or all hardware resource are idling
  if (fetch_index <= sim_insn) return false;

  if (instr_queue_size > 0) return false;

  for (int i = 0; i < RESERV_INT_SIZE; i++) {
    if (reservINT[i] != NULL) return false;
  }

  for (int i = 0; i < RESERV_FP_SIZE; i++) {
    if (reservFP[i] != NULL) return false;
  }

  for (int i = 0; i < FU_INT_SIZE; i++) {
    if (fuINT[i] != NULL) return false;
  }

  for (int i = 0; i < FU_FP_SIZE; i++) {
    if (fuFP[i] != NULL) return false;
  }

  if (commonDataBus != NULL) return false;

  return true; //ECE552: you can change this as needed; we've added this so the code provided to you compiles

  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Retires the instruction from writing to the Common Data Bus
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void CDB_To_retire(int current_cycle) {

  /* ECE552 Assignment 3 - BEGIN CODE */
  if (!commonDataBus || !WRITES_CDB(commonDataBus->op)) return;

  // update RS for both FP and INT
  for(int i=0; i<RESERV_FP_SIZE; i++){
    if(reservFP[i] == NULL) continue;
    for (int j=0; j<3; j++) {
      if (reservFP[i]->Q[j] == commonDataBus) {
        reservFP[i]->Q[j] = NULL;
      }
    }
  }
  
  for(int i=0; i<RESERV_INT_SIZE; i++){
    if(reservINT[i] == NULL) continue;
    for (int j=0; j<3; j++) {
      if (reservINT[i]->Q[j] == commonDataBus) {
        reservINT[i]->Q[j] = NULL;
      }
    }
  }

  // Update map table
  if(commonDataBus->r_out[0] != DNA && map_table[commonDataBus->r_out[0]] == commonDataBus) map_table[commonDataBus->r_out[0]] = NULL;
  if(commonDataBus->r_out[1] != DNA && map_table[commonDataBus->r_out[1]] == commonDataBus) map_table[commonDataBus->r_out[1]] = NULL;

  commonDataBus->tom_cdb_cycle = current_cycle;
  commonDataBus = NULL;
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {
  /* ECE552 Assignment 3 - BEGIN CODE */
  commonDataBus = NULL;
  int earliest_dispatch = INT_MAX;
  instruction_t** functional_unit = NULL;
  int index = 0;

  // Check floating point functional units
  for (int i = 0; i < FU_FP_SIZE; i++) {
    instruction_t* current_instr = fuFP[i];
    if(current_instr == NULL) continue;
    if ((current_cycle - current_instr->tom_execute_cycle + 1) >= FU_FP_LATENCY) {
      if (earliest_dispatch > current_instr->tom_dispatch_cycle) {
        earliest_dispatch = current_instr->tom_dispatch_cycle;
        commonDataBus = current_instr;
        index = i;
        functional_unit = fuFP;
      }
    }
  }

  // Check integer functional units
  for (int i = 0; i < FU_INT_SIZE; i++) {
    instruction_t* current_instr = fuINT[i];
    if(current_instr == NULL) continue;
    if ((current_cycle - current_instr->tom_execute_cycle + 1) >= FU_INT_LATENCY) {
      if (IS_STORE(current_instr->op)) {
        fuINT[i] = NULL;
        for(int j=0; j<RESERV_INT_SIZE; j++){
          if(reservINT[j] == current_instr){
            reservINT[j] = NULL;
            break;
          }
        }
        current_instr->tom_cdb_cycle = 0;
        continue;
      }

      if (earliest_dispatch > current_instr->tom_dispatch_cycle) {
        earliest_dispatch = current_instr->tom_dispatch_cycle;
        commonDataBus = current_instr;
        index = i;
        functional_unit = fuINT;
      }
    }
  }

  if(commonDataBus == NULL) return;

  // Clear functional unit entry
  functional_unit[index] = NULL;

  // Clear reservation station entry
  for(int i=0; i<RESERV_INT_SIZE; i++){
    if(reservINT[i] == commonDataBus){
      reservINT[i] = NULL;
      return;
    }
  }
  for(int i=0; i<RESERV_FP_SIZE; i++){
    if(reservFP[i] == commonDataBus){
      reservFP[i] = NULL;
      return;
    }
  }
  /* ECE552 Assignment 3 - END CODE */
}

void issue_to_execute_helper(int current_cycle, instruction_t** reserv, int size, instruction_t** fu, int fu_size){
  /* ECE552 Assignment 3 - BEGIN CODE */
  int count = 0;
  int* cycle_array = (int*)malloc(sizeof(int) * size);
  instruction_t** instr_array = (instruction_t**)malloc(sizeof(instruction_t*) * size);
  for(int i = 0; i < size; i++){
    instruction_t* instr = reserv[i];
    if(instr == NULL || instr->tom_execute_cycle > 0 || instr->tom_issue_cycle == current_cycle) continue;
    bool ready_to_execute = true;
    for(int j = 0; j < 3; j++){
      instruction_t* q = instr->Q[j];
      if(q != NULL){
        ready_to_execute = false;
        break;
      }
    }
    if(ready_to_execute){
      cycle_array[count] = instr->tom_dispatch_cycle;
      instr_array[count] = instr;
      count++;
      insert_into_ordered_array(cycle_array, instr_array, count);
    }
  }

  for(int i = 0; i < count; i++){
    int available_index = find_available_index(fu, fu_size);
    instruction_t* instr = instr_array[i];
    if(available_index >= 0){
      instr->tom_execute_cycle = current_cycle;
      fu[available_index] = instr;
    }
  }
  free(cycle_array);
  free(instr_array);
  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the issue to the execute stage (if possible). We prioritize old instructions
 *      (in program order) over new ones, if they both contend for the same functional unit.
 *      All RAW dependences need to have been resolved with stalls before an instruction enters execute.
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
/* ECE552 Assignment 3 - BEGIN CODE */
void issue_To_execute(int current_cycle) {
  issue_to_execute_helper(current_cycle, reservFP, RESERV_FP_SIZE, fuFP, FU_FP_SIZE);
  issue_to_execute_helper(current_cycle, reservINT, RESERV_INT_SIZE, fuINT, FU_INT_SIZE);
}
/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
/* ECE552 Assignment 3 - BEGIN CODE */
void dispatch_To_issue(int current_cycle) {
  instruction_t* instr = instr_queue[0];
  if(instr == NULL || instr->tom_dispatch_cycle == current_cycle) return;

  int available_index = -1;
  instruction_t** reserv = NULL;
  if(USES_FP_FU(instr->op)){
    available_index = find_available_index((instruction_t*)reservFP, RESERV_FP_SIZE);
    reserv = reservFP;
  }
  else if(USES_INT_FU(instr->op)){
    available_index = find_available_index((instruction_t*)reservINT, RESERV_INT_SIZE);
    reserv = reservINT;
  }
  else{
    remove_IFQ_head();
    return;
  }

  if(available_index < 0) return;
  reserv[available_index] = instr;
  instr->tom_issue_cycle = current_cycle;
  remove_IFQ_head();

  for(int j = 0; j < 3; j++){
    int reg = instr->r_in[j];
    if(reg == DNA) continue;
    instruction_t* tag = map_table[reg];
    if(tag != NULL){
      instr->Q[j] = tag;
    }
  }

  if(IS_STORE(instr->op)) return; 
  for(int j = 0; j < 2; j++){
    int reg = instr->r_out[j];
    if(reg == DNA) continue;
    map_table[reg] = instr;
  }
}
/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */
/* ECE552 Assignment 3 - BEGIN CODE */
void fetch(instruction_trace_t* trace) {
  if(fetch_index > sim_num_insn) return;
  if(instr_queue_size < INSTR_QUEUE_SIZE){
    do{
      instr_queue[instr_queue_size] = get_instr(trace, fetch_index);
      fetch_index++;
    } while(IS_TRAP(instr_queue[instr_queue_size]->op) && fetch_index <= sim_num_insn);
    if(IS_TRAP(instr_queue[instr_queue_size]->op)) return;
    instr_queue_size++;
  }
}

void remove_IFQ_head(){
  for(int i = 0; i < instr_queue_size; i++){
    if(i == INSTR_QUEUE_SIZE-1){
      instr_queue[i] = NULL;
      break;
    }
    instr_queue[i] = instr_queue[i+1];
  }
  instr_queue_size--;
}
/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Calls fetch and dispatches an instruction at the same cycle (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
/* ECE552 Assignment 3 - BEGIN CODE */
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {
  fetch(trace);
  if(instr_queue[0] == NULL) return;
  for(int i = 0; i < instr_queue_size; i++){
    if(instr_queue[i] == NULL) break;
    if(instr_queue[i]->tom_dispatch_cycle <= 0) instr_queue[i]->tom_dispatch_cycle = current_cycle;
  }
}
/* ECE552 Assignment 3 - END CODE */

/* 
 * Description: 
 * 	Performs a cycle-by-cycle simulation of the 4-stage pipeline
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	The total number of cycles it takes to execute the instructions.
 * Extra Notes:
 * 	sim_num_insn: the number of instructions in the trace
 */
counter_t runTomasulo(instruction_trace_t* trace)
{
  //initialize instruction queue
  int i;
  for (i = 0; i < INSTR_QUEUE_SIZE; i++) {
    instr_queue[i] = NULL;
  }

  //initialize reservation stations
  for (i = 0; i < RESERV_INT_SIZE; i++) {
      reservINT[i] = NULL;
  }

  for(i = 0; i < RESERV_FP_SIZE; i++) {
      reservFP[i] = NULL;
  }

  //initialize functional units
  for (i = 0; i < FU_INT_SIZE; i++) {
    fuINT[i] = NULL;
  }

  for (i = 0; i < FU_FP_SIZE; i++) {
    fuFP[i] = NULL;
  }

  //initialize map_table to no producers
  int reg;
  for (reg = 0; reg < MD_TOTAL_REGS; reg++) {
    map_table[reg] = NULL;
  }
  
  int cycle = 1;
  while (true) {
    /* ECE552 Assignment 3 - BEGIN CODE */
    fetch_To_dispatch(trace, cycle);
    dispatch_To_issue(cycle);
    issue_To_execute(cycle);
    CDB_To_retire(cycle);
    execute_To_CDB(cycle);
    /* ECE552 Assignment 3 - END CODE */

    cycle++;

    if (is_simulation_done(sim_num_insn))
      break;

  }
  
  return cycle;
}
