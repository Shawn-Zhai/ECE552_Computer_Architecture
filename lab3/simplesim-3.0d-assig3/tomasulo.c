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

//the index of the last instruction fetched
static int fetch_index = 0;

/* FUNCTIONAL UNITS */


/* RESERVATION STATIONS */


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

  // done only if all instructions are finished processing or 
  // all hardware resource are idling
  if (fetch_index < sim_insn) return false;

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

  // CDB must start and end on same cycle
  // tom_cdb_cycle is set to be (exe_finish_cycle + 1) but runs in the same
  // cycle as execution finishes. But invoking CDB_To_retire before execute_To_CDB
  // effectively delay it by one more cycle
  // assert(commonDataBus->tom_cdb_cycle == current_cycle);

  // update INT RS
  for (int i = 0; i < RESERV_INT_SIZE; i++){

    if (reservINT[i] == NULL) continue;

    for (int j = 0; j < 3; j++) {

      if (reservINT[i]->Q[j] == commonDataBus) {

        reservINT[i]->Q[j] = NULL;
      }
    }
  }

  // update FP RS
  for (int i=0; i<RESERV_FP_SIZE; i++){

    if (reservFP[i] == NULL) continue;

    for (int j=0; j<3; j++) {

      if (reservFP[i]->Q[j] == commonDataBus) {

        reservFP[i]->Q[j] = NULL;
      }
    }
  }

  // update map table
  for (int i = 0; i < MD_TOTAL_REGS; i++){

    if (map_table[i] == commonDataBus){

      map_table[i] = NULL;
    }
  }

  commonDataBus = NULL;
  /* ECE552 Assignment 3 - END CODE */
}

/* ECE552 Assignment 3 - BEGIN CODE */
void free_store(int current_cycle) {

  for (int i = 0; i < FU_INT_SIZE; i++) {

    instruction_t* instr = fuINT[i];
    if(instr && IS_STORE(instr->op) && 
       current_cycle - instr->tom_execute_cycle >= FU_INT_LATENCY - 1) {
      
      fuINT[i] = NULL;
      for(int j=0; j<RESERV_INT_SIZE; j++){

        if(reservINT[j] == instr){

          reservINT[j] = NULL;
          break;
        }
      }
        
      instr->tom_cdb_cycle = 0;
    }
  }
}

typedef enum {
  FU_NONE,
  FU_INT,
  FU_FP
} FU_Type;

/* 
 * Description: 
 * 	Moves an instruction from the execution stage to common data bus (if possible)
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void execute_To_CDB(int current_cycle) {

  // if a STORE instr finishes executing, it doesn't go to CDB
  // clear its RS and FU entry right away
  free_store(current_cycle);

  // wait if CDB unavailable
  if (commonDataBus) return;

  FU_Type FU_type = FU_NONE; // 0 if INT instr goes to CDB, 1 if FP
  int index = -1;
  int dispatch_cycle = INT_MAX; // to find the oldest instr in contention

  // check INT FU
  for (int i = 0; i < FU_INT_SIZE; i++) {

    instruction_t* instr = fuINT[i];
    if (!instr) continue;

    // done executing and is older
    if (current_cycle - instr->tom_execute_cycle >= FU_INT_LATENCY - 1 &&
        instr->tom_dispatch_cycle < dispatch_cycle) {

      commonDataBus = instr;
      FU_type = FU_INT;
      index = i;
      dispatch_cycle = instr->tom_dispatch_cycle;
    }
  }

  // check FP FU
  for (int i = 0; i < FU_FP_SIZE; i++) {

    instruction_t* instr = fuFP[i];
    if (!instr) continue;

    // done executing and is older
    if (current_cycle - instr->tom_execute_cycle >= FU_FP_LATENCY - 1 &&
        instr->tom_dispatch_cycle < dispatch_cycle) {

      commonDataBus = instr;
      FU_type = FU_FP;
      index = i;
      dispatch_cycle = instr->tom_dispatch_cycle;
    }
  }

  if (commonDataBus == NULL || FU_type == FU_NONE) return;

  // finish execution at current cycle, start CDB next cycle
  commonDataBus->tom_cdb_cycle = current_cycle + 1;

  // Clear FU and RS entry of the instr going to CDB
  if (FU_type == FU_INT) {

    fuINT[index] = NULL;

    for (int i=0; i<RESERV_INT_SIZE; i++){

      if (reservINT[i] == commonDataBus){

        reservINT[i] = NULL;
        return;
      }
    }
  }

  else if (FU_type == FU_FP) {
    
    fuFP[index] = NULL;

    for (int i=0; i<RESERV_FP_SIZE; i++){

      if (reservFP[i] == commonDataBus){

        reservFP[i] = NULL;
        return;
      }
    }
  }
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
void issue_To_execute(int current_cycle) {

  /* ECE552 Assignment 3 - BEGIN CODE */

  // check INT instr
  for (int i = 0; i < FU_INT_SIZE; i++) {

    if (fuINT[i]) continue;

    // found available INT FU
    instruction_t* oldest_instr = NULL;
    int dispatch_cycle = INT_MAX;

    // find oldest INT instr that's executable
    for (int j = 0; j < RESERV_INT_SIZE; j++) {

      instruction_t* instr = reservINT[j];

      // look for instr that has not yet start execution and is not issued in the same cycle
      if(!instr || instr->tom_execute_cycle > 0 || instr->tom_issue_cycle == current_cycle) continue;

      // check if all dependencies are ready
      bool issue_end = true;
      for (int k = 0; k < 3; k++) {

        if (instr->Q[k]) {

          issue_end = false;
          break;
        }
      }

      // set oldest_instr if instr is executable in the next cycle and older
      if (issue_end && instr->tom_dispatch_cycle < dispatch_cycle) {

        oldest_instr = instr;
        dispatch_cycle = instr->tom_dispatch_cycle;
      }
    }

    // execute the oldest
    if (oldest_instr) {

      fuINT[i] = oldest_instr;

      // issue ends at current cycle, execution starts next cycle
      oldest_instr->tom_execute_cycle = current_cycle;
    }
    else break;
  }

  // check FP instr
  for (int i = 0; i < FU_FP_SIZE; i++) {

    if (fuFP[i]) continue;

    // found available FP FU
    instruction_t* oldest_instr = NULL;
    int dispatch_cycle = INT_MAX;

    // find oldest FP instr that's executable
    for (int j = 0; j < RESERV_FP_SIZE; j++) {

      instruction_t* instr = reservFP[j];

      // look for instr that has not yet start execution and is not issued in the same cycle
      if(!instr || instr->tom_execute_cycle > 0 || instr->tom_issue_cycle == current_cycle) continue;

      // check if all dependencies are ready
      bool issue_end = true;
      for (int k = 0; k < 3; k++) {

        if (instr->Q[k]) {

          issue_end = false;
          break;
        }
      }

      // set oldest_instr if instr is executable in the next cycle and older
      if (issue_end && instr->tom_dispatch_cycle < dispatch_cycle) {

        oldest_instr = instr;
        dispatch_cycle = instr->tom_dispatch_cycle;
      }
    }

    // execute the oldest
    if (oldest_instr) {

      fuFP[i] = oldest_instr;

      // issue ends at current cycle, execution starts next cycle
      oldest_instr->tom_execute_cycle = current_cycle;
    }
    else break;
  }

  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Moves instruction(s) from the dispatch stage to the issue stage
 * Inputs:
 * 	current_cycle: the cycle we are at
 * Returns:
 * 	None
 */
void dispatch_To_issue(int current_cycle) {

  /* ECE552 Assignment 3 - BEGIN CODE */

  instruction_t* instr = instr_queue[0];

  // cannot issue at the same cycle as dispatch
  if(instr == NULL || instr->tom_dispatch_cycle == current_cycle) return;

  bool RS_inserted = false;
  if (USES_INT_FU(instr->op)) {

    for (int i = 0; i < RESERV_INT_SIZE; i++) {

      if (!reservINT[i]) {
        
        // dispatch completed earlier, issue starts at current cycle
        instr->tom_issue_cycle = current_cycle;
        reservINT[i] = instr;
        RS_inserted = true;
        break;
      }
    }
  }
  else if (USES_FP_FU(instr->op)) {

    for (int i = 0; i < RESERV_FP_SIZE; i++) {

      if (!reservFP[i]) {
        
        // dispatch completed earlier, issue starts at current cycle
        instr->tom_issue_cycle = current_cycle;
        reservFP[i] = instr;
        RS_inserted = true;
        break;
      }
    }
  }

  // pop IFQ if instr IS_COND_CTRL or IS_UNCOND_CTRL or issued to RS
  if (IS_COND_CTRL(instr->op) || IS_UNCOND_CTRL(instr->op) || RS_inserted) {

    for(int i = 0; i < instr_queue_size - 1; i++) {

      instr_queue[i] = instr_queue[i+1];
    }

    instr_queue[instr_queue_size - 1] = NULL;
    instr_queue_size--;
  }

  // update dependencies and map table if instr issued
  if (RS_inserted) {

    for(int i = 0; i < 3; i++){
      
      int ireg = instr -> r_in[i];
      if(ireg != DNA && map_table[ireg]) {

        instr -> Q[i] = map_table[ireg];
      }
    }

    // STORE does not update map table
    if(!IS_STORE(instr->op)) {

      for(int i = 0; i < 2; i++){

        int oreg = instr -> r_out[i];
        if(oreg != DNA) {

          map_table[oreg] = instr;
        }
      }
    }
  }

  /* ECE552 Assignment 3 - END CODE */
}

/* 
 * Description: 
 * 	Grabs an instruction from the instruction trace (if possible)
 * Inputs:
 *      trace: instruction trace with all the instructions executed
 * Returns:
 * 	None
 */

/* ECE552 Assignment 3 - BEGIN CODE */
void fetch(instruction_trace_t* trace, int current_cycle) {

  if(fetch_index < sim_num_insn && instr_queue_size < INSTR_QUEUE_SIZE) {

    // fetch until getting a non TRAP or reaching 1000000
    instruction_t* fetched_instr = NULL;
    do {
      fetched_instr = get_instr(trace, ++fetch_index);
    } while(IS_TRAP(fetched_instr->op) && fetch_index < sim_num_insn);
    
    // in case the 1000000th instr is a TRAP
    if (!IS_TRAP(fetched_instr->op)) {
      
      // instr starts dispatch stage once it gets into IFQ
      fetched_instr->tom_dispatch_cycle = current_cycle;
      instr_queue[instr_queue_size] = fetched_instr;
      instr_queue_size++;
    }
  }
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
void fetch_To_dispatch(instruction_trace_t* trace, int current_cycle) {

  /* ECE552 Assignment 3 - BEGIN CODE */
  
  fetch(trace, current_cycle);
  
  /* ECE552 Assignment 3 - END CODE */
}

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
    // CDB_To_retire(cycle);
    fetch_To_dispatch(trace, cycle);
    dispatch_To_issue(cycle);
    issue_To_execute(cycle);

    // CDB_To_retire runs before execute_To_CDB effectively models the situation 
    // where an instruction finish EXECUTION stage in cycle 10 and uses CDB in cycle 11.
    // Retiring CDB after issue_To_execute prevents an instruction waiting on the 
    // CDB broadcast to finish ISSUE stage and start EXECUTION stage in the same cycle.
    CDB_To_retire(cycle);
    execute_To_CDB(cycle);
    /* ECE552 Assignment 3 - END CODE */

    cycle++;

    if (is_simulation_done(sim_num_insn))
      break;

  }
  
  return cycle;
}
