/* sim-safe.c - sample functional simulator implementation */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


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


/* ECE552 Assignment 1 - STATS COUNTERS - BEGIN */
static counter_t sim_num_RAW_hazard_q1 = 0;
static counter_t sim_num_RAW_hazard_q1_1stall = 0;
static counter_t sim_num_RAW_hazard_q1_2stall = 0;
static counter_t reg_ready_q1[MD_TOTAL_REGS];

static counter_t sim_num_RAW_hazard_q2 = 0;
static counter_t sim_num_RAW_hazard_q2_1stall = 0;
static counter_t sim_num_RAW_hazard_q2_2stall = 0;
static counter_t reg_ready_q2[MD_TOTAL_REGS];
/* ECE552 Assignment 1 - STATS COUNTERS - END */

/*
 * This file implements a functional simulator.  This functional simulator is
 * the simplest, most user-friendly simulator in the simplescalar tool set.
 * Unlike sim-fast, this functional simulator checks for all instruction
 * errors, and the implementation is crafted for clarity rather than speed.
 */

/* simulated registers */
static struct regs_t regs;

/* simulated memory */
static struct mem_t *mem = NULL;

/* track number of refs */
static counter_t sim_num_refs = 0;

/* maximum number of inst's to execute */
static unsigned int max_insts;

/* register simulator-specific options */
void
sim_reg_options(struct opt_odb_t *odb)
{
  opt_reg_header(odb, 
"sim-safe: This simulator implements a functional simulator.  This\n"
"functional simulator is the simplest, most user-friendly simulator in the\n"
"simplescalar tool set.  Unlike sim-fast, this functional simulator checks\n"
"for all instruction errors, and the implementation is crafted for clarity\n"
"rather than speed.\n"
		 );

  /* instruction limit */
  opt_reg_uint(odb, "-max:inst", "maximum number of inst's to execute",
	       &max_insts, /* default */0,
	       /* print */TRUE, /* format */NULL);

}

/* check simulator-specific option values */
void
sim_check_options(struct opt_odb_t *odb, int argc, char **argv)
{
  /* nada */
}

/* register simulator-specific statistics */
void
sim_reg_stats(struct stat_sdb_t *sdb)
{
  stat_reg_counter(sdb, "sim_num_insn",
		   "total number of instructions executed",
		   &sim_num_insn, sim_num_insn, NULL);
  stat_reg_counter(sdb, "sim_num_refs",
		   "total number of loads and stores executed",
		   &sim_num_refs, 0, NULL);
  stat_reg_int(sdb, "sim_elapsed_time",
	       "total simulation time in seconds",
	       &sim_elapsed_time, 0, NULL);
  stat_reg_formula(sdb, "sim_inst_rate",
		   "simulation speed (in insts/sec)",
		   "sim_num_insn / sim_elapsed_time", NULL);

  /* ECE552 Assignment 1 - BEGIN CODE */

  stat_reg_counter(sdb, "sim_num_RAW_hazard_q1",
		   "total number of RAW hazards (q1)",
		   &sim_num_RAW_hazard_q1, sim_num_RAW_hazard_q1, NULL);

  stat_reg_counter(sdb, "sim_num_RAW_hazard_q1_1stall",
		   "total number of 1 cycle stall RAW hazards (q1)",
		   &sim_num_RAW_hazard_q1_1stall, sim_num_RAW_hazard_q1_1stall, NULL);

  stat_reg_counter(sdb, "sim_num_RAW_hazard_q1_2stall",
		   "total number of 2 cycle stall RAW hazards (q1)",
		   &sim_num_RAW_hazard_q1_2stall, sim_num_RAW_hazard_q1_2stall, NULL);

  stat_reg_counter(sdb, "sim_num_RAW_hazard_q2",
		   "total number of RAW hazards (q2)",
		   &sim_num_RAW_hazard_q2, sim_num_RAW_hazard_q2, NULL);

  stat_reg_counter(sdb, "sim_num_RAW_hazard_q2_1stall",
		   "total number of 1 cycle stall RAW hazards (q2)",
		   &sim_num_RAW_hazard_q2_1stall, sim_num_RAW_hazard_q2_1stall, NULL);

  stat_reg_counter(sdb, "sim_num_RAW_hazard_q2_2stall",
		   "total number of 2 cycle stall RAW hazards (q2)",
		   &sim_num_RAW_hazard_q2_2stall, sim_num_RAW_hazard_q2_2stall, NULL);

  // CPI = 1 + total stall cycles / total instructions

  stat_reg_formula(sdb, "CPI_from_RAW_hazard_q1",
		   "CPI from RAW hazard (q1)",
		   "1 + (sim_num_RAW_hazard_q1_1stall + 2 * sim_num_RAW_hazard_q1_2stall) / sim_num_insn" /* ECE552 - MUST ADD YOUR FORMULA */, NULL);

  stat_reg_formula(sdb, "CPI_from_RAW_hazard_q2",
		   "CPI from RAW hazard (q2)",
		   "1 + (sim_num_RAW_hazard_q2_1stall + 2 * sim_num_RAW_hazard_q2_2stall) / sim_num_insn" /* ECE552 - MUST ADD YOUR FORMULA */, NULL);

  /* ECE552 Assignment 1 - END CODE */

  ld_reg_stats(sdb);
  mem_reg_stats(mem, sdb);
}

/* initialize the simulator */
void
sim_init(void)
{
  sim_num_refs = 0;

  /* allocate and initialize register file */
  regs_init(&regs);

  /* allocate and initialize memory space */
  mem = mem_create("mem");
  mem_init(mem);
}

/* load program into simulated state */
void
sim_load_prog(char *fname,		/* program to load */
	      int argc, char **argv,	/* program arguments */
	      char **envp)		/* program environment */
{
  /* load program text and data, set up environment, memory, and regs */
  ld_load_prog(fname, argc, argv, envp, &regs, mem, TRUE);

  /* initialize the DLite debugger */
  dlite_init(md_reg_obj, dlite_mem_obj, dlite_mstate_obj);
}

/* print simulator-specific configuration information */
void
sim_aux_config(FILE *stream)		/* output stream */
{
  /* nothing currently */
}

/* dump simulator-specific auxiliary simulator statistics */
void
sim_aux_stats(FILE *stream)		/* output stream */
{
  /* nada */
}

/* un-initialize simulator-specific state */
void
sim_uninit(void)
{
  /* nada */
}


/*
 * configure the execution engine
 */

/*
 * precise architected register accessors
 */

/* next program counter */
#define SET_NPC(EXPR)		(regs.regs_NPC = (EXPR))

/* current program counter */
#define CPC			(regs.regs_PC)

/* general purpose registers */
#define GPR(N)			(regs.regs_R[N])
#define SET_GPR(N,EXPR)		(regs.regs_R[N] = (EXPR))

#define DNA (0)

#if defined(TARGET_PISA)

/* general register dependence decoders */
#define DGPR(N)			(N)
#define DGPR_D(N)		((N) &~1)

/* floating point register dependence decoders */
#define DFPR_L(N)		(((N)+32)&~1)
#define DFPR_F(N)		(((N)+32)&~1)
#define DFPR_D(N)		(((N)+32)&~1)

/* miscellaneous register dependence decoders */
#define DHI			(0+32+32)
#define DLO			(1+32+32)
#define DFCC			(2+32+32)
#define DTMP			(3+32+32)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_L(N)		(regs.regs_F.l[(N)])
#define SET_FPR_L(N,EXPR)	(regs.regs_F.l[(N)] = (EXPR))
#define FPR_F(N)		(regs.regs_F.f[(N)])
#define SET_FPR_F(N,EXPR)	(regs.regs_F.f[(N)] = (EXPR))
#define FPR_D(N)		(regs.regs_F.d[(N) >> 1])
#define SET_FPR_D(N,EXPR)	(regs.regs_F.d[(N) >> 1] = (EXPR))

/* miscellaneous register accessors */
#define SET_HI(EXPR)		(regs.regs_C.hi = (EXPR))
#define HI			(regs.regs_C.hi)
#define SET_LO(EXPR)		(regs.regs_C.lo = (EXPR))
#define LO			(regs.regs_C.lo)
#define FCC			(regs.regs_C.fcc)
#define SET_FCC(EXPR)		(regs.regs_C.fcc = (EXPR))

#elif defined(TARGET_ALPHA)

/* floating point registers, L->word, F->single-prec, D->double-prec */
#define FPR_Q(N)		(regs.regs_F.q[N])
#define SET_FPR_Q(N,EXPR)	(regs.regs_F.q[N] = (EXPR))
#define FPR(N)			(regs.regs_F.d[(N)])
#define SET_FPR(N,EXPR)		(regs.regs_F.d[(N)] = (EXPR))

/* miscellaneous register accessors */
#define FPCR			(regs.regs_C.fpcr)
#define SET_FPCR(EXPR)		(regs.regs_C.fpcr = (EXPR))
#define UNIQ			(regs.regs_C.uniq)
#define SET_UNIQ(EXPR)		(regs.regs_C.uniq = (EXPR))

#else
#error No ISA target defined...
#endif

/* precise architected memory state accessor macros */
#define READ_BYTE(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_BYTE(mem, addr))
#define READ_HALF(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_HALF(mem, addr))
#define READ_WORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_WORD(mem, addr))
#ifdef HOST_HAS_QWORD
#define READ_QWORD(SRC, FAULT)						\
  ((FAULT) = md_fault_none, addr = (SRC), MEM_READ_QWORD(mem, addr))
#endif /* HOST_HAS_QWORD */

#define WRITE_BYTE(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_BYTE(mem, addr, (SRC)))
#define WRITE_HALF(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_HALF(mem, addr, (SRC)))
#define WRITE_WORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_WORD(mem, addr, (SRC)))
#ifdef HOST_HAS_QWORD
#define WRITE_QWORD(SRC, DST, FAULT)					\
  ((FAULT) = md_fault_none, addr = (DST), MEM_WRITE_QWORD(mem, addr, (SRC)))
#endif /* HOST_HAS_QWORD */

/* system call handler macro */
#define SYSCALL(INST)	sys_syscall(&regs, mem_access, mem, INST, TRUE)

/* start simulation, program loaded, processor precise state initialized */
void
sim_main(void)
{
  /* ECE552 Assignment 1 - BEGIN CODE*/
  int r_out[2], r_in[3];
  /* ECE552 Assignment 1 - END CODE*/

  md_inst_t inst;
  register md_addr_t addr;
  enum md_opcode op;
  register int is_write;
  enum md_fault_type fault;

  fprintf(stderr, "sim: ** starting functional simulation **\n");

  /* set up initial default next PC */
  regs.regs_NPC = regs.regs_PC + sizeof(md_inst_t);

  /* check for DLite debugger entry condition */
  if (dlite_check_break(regs.regs_PC, /* !access */0, /* addr */0, 0, 0))
    dlite_main(regs.regs_PC - sizeof(md_inst_t),
	       regs.regs_PC, sim_num_insn, &regs, mem);

  while (TRUE)
    {

      /* maintain $r0 semantics */
      regs.regs_R[MD_REG_ZERO] = 0;
#ifdef TARGET_ALPHA
      regs.regs_F.d[MD_REG_ZERO] = 0.0;
#endif /* TARGET_ALPHA */

      /* get the next instruction to execute */
      MD_FETCH_INST(inst, mem, regs.regs_PC);

      /* keep an instruction count */
      sim_num_insn++;

      /* set default reference address and access mode */
      addr = 0; is_write = FALSE;

      /* set default fault - none */
      fault = md_fault_none;

      /* decode the instruction */
      MD_SET_OPCODE(op, inst);

      /* execute the instruction */

      switch (op)
	{
  /* ECE552 Assignment 1 - BEGIN CODE*/
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
  case OP:							\
    r_out[0] = (O1); r_out[1] = (O2); \
    r_in[0] = (I1); r_in[1] = (I2); r_in[2] = (I3); \
    SYMCAT(OP,_IMPL);						\
    break; 
  /* ECE552 Assignment 1 - END CODE*/
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
        case OP:							\
          panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#define DECLARE_FAULT(FAULT)						\
	  { fault = (FAULT); break; }
#include "machine.def"
	default:
	  panic("attempted to execute a bogus opcode");
      }

      if (fault != md_fault_none)
	fatal("fault (%d) detected @ 0x%08p", fault, regs.regs_PC);

      if (verbose)
	{
	  myfprintf(stderr, "%10n [xor: 0x%08x] @ 0x%08p: ",
		    sim_num_insn, md_xor_regs(&regs), regs.regs_PC);
	  md_print_insn(inst, regs.regs_PC, stderr);
	  if (MD_OP_FLAGS(op) & F_MEM)
	    myfprintf(stderr, "  mem: 0x%08p", addr);
	  fprintf(stderr, "\n");
	  /* fflush(stderr); */
	}

      if (MD_OP_FLAGS(op) & F_MEM)
	{
	  sim_num_refs++;
	  if (MD_OP_FLAGS(op) & F_STORE)
	    is_write = TRUE;
	}

    /* ECE552 Assignment 1 - BEGIN CODE*/ 
      // Question 1
      {
        unsigned int max_stall = 0;

        // Go through all input registers. if more than one of them causes RAW hazard, we treat them as one hazard and count the highest number of stall cycles
        for(int i = 0; i<3; i++){

          // Check if the current instruction is reading from a register that's not ready yet
          if (r_in[i] != DNA && reg_ready_q1[r_in[i]] > sim_num_insn){

            // The stall cycles required for the current registed being examined is the difference between the current instruction
            // number and the instruction number where this register will be ready
            int stall = reg_ready_q1[r_in[i]] - sim_num_insn;

            /*
            1 stall cycle senario:
            Write to reg -> Irrelevant Instruction -> Read from the same reg
            The Read instruction needs to stall for 1 cycle

            2 stall cycles senario:
            Write to reg -> Read from the same reg
            The Read instruction needs to stall for 2 cycle
            */

            if(stall == 2) {
              max_stall = stall;
              break;
            }

            // Save the max stall cycles
            if(stall > max_stall)
              max_stall = stall;
          }
        }

        if(max_stall > 0){

          if(max_stall == 1){
            sim_num_RAW_hazard_q1_1stall++;
          }
          
          if(max_stall == 2){
            sim_num_RAW_hazard_q1_2stall++;
          }

          sim_num_RAW_hazard_q1++;

          /*
          Here, we address two corner cases.

          Corner case 1:

          INST_NUM    Instruction                   Readiness Table:
          1000        Write to R0                   R0: 1003
          1001        Read1 from R0
          1002        Read2 from R0

          In this senario, since we simulate stall cycle based purely on the instruction number, Read1 will need to stall for 2 cycles and Read2 will need to stall for 1 cycle.
          However, after Read1 finishes stalling and starts to be processed as the "1003rd" instruction, Read2 will then be processed as the "1004th" instruction. At this stage
          it no longer needs to stall. Therefore, we should not consider Read2 as a RAW hazard nor counting its stall cycle.

          Corner case 2:

          INST_NUM    Instruction                   Readiness Table:
          1000        Write1 to R0                  R0: 1003
          1001        Write1 to R1                  R1: 1004
          1002        Read1 from R0
          1003        Read2 from R1

          In this senario, since we simulate stall cycle based purely on the instruction number, Read1 will need to stall for 1 cycle and Read2 will also need to stall for 1 cycle.
          However, after Read1 finishes stalling and starts to be processed as the "1003rd" instruction, Read2 will then be processed as the "1004th" instruction. At this stage
          it no longer needs to stall. Therefore, we should not consider Read2 as a RAW hazard nor counting its stall cycle.


          In conclusion, to address the two corner case stated above, we shift the instruction number for all registers in the readiness table earlier by the number of stall cycles
          required by the current instruction being examined. This effectively counts the stalling time of the current instruction towards the time that future instructions wait for
          the registers they depend on to be ready.
          */
          for(int i = 0; i<MD_TOTAL_REGS; i++){
            reg_ready_q1[i] -= max_stall;
          }
        }

        // Update the readiness of any registers being written to with (current instruction number + 3), which means that it will be ready to use at the third instruction after the 
        // current one.
        if (r_out[0] != DNA)
          reg_ready_q1[r_out[0]] = sim_num_insn + 3;
        if (r_out[1] != DNA)
          reg_ready_q1[r_out[1]] = sim_num_insn + 3;
      }

      // Question 2
      {
        unsigned int max_stall = 0;

        // Go through all input registers. if more than one of them causes RAW hazard, we treat them as one hazard and count the highest number of stall cycles
        for(int i = 0; i<3; i++){

          /* 
          Corner case:
          If the instruction a STORE, I1(r_in[0]) holds the value to be stored. The latest pipeling stage Its value needs to be ready at is the memory stage. In the 6-stage pipeline
          architecture we are considering, this can always be achieved by forwarding in all senarios. Therefore, STORE instructions never require stalling.
          */
          if(i == 0 && MD_OP_FLAGS(op) & F_MEM && MD_OP_FLAGS(op) & F_STORE){
            continue;
          }
          
          // Check if the current instruction is reading from a register that's not ready yet
          if (r_in[i] != DNA && reg_ready_q2[r_in[i]] > sim_num_insn){

            // The stall cycles required for the current registed being examined is the difference between the current instruction
            // number and the instruction number where this register will be ready
            int stall = reg_ready_q2[r_in[i]] - sim_num_insn;

            /*
            1 stall cycle senario:
            Write to reg -> Read from the same reg
            The Read instruction needs to stall for 1 cycle

            OR

            LOAD to reg -> Irrelevant Instruction -> Read from the same reg
            The Read instruction needs to stall for 1 cycle

            2 stall cycles senario:
            LOAD to reg -> Read from the same reg
            The Read instruction needs to stall for 2 cycle
            */

            if(stall == 2) {
              max_stall = stall;
              break;
            }

            // Save the max stall cycles
            if(stall > max_stall){
              max_stall = stall;
            }
          }
        }

        if(max_stall > 0){
          if(max_stall == 1){
            sim_num_RAW_hazard_q2_1stall++;
          }
          if(max_stall == 2){
            sim_num_RAW_hazard_q2_2stall++;
          }
          sim_num_RAW_hazard_q2++;

          /*
          Here, we address two corner cases using the same methodology in Q1.

          Corner case 1:

          INST_NUM    Instruction                   Readiness Table:
          1000        Load to R0                    R0: 1003
          1001        Read1 from R0
          1002        Read2 from R0

          In this senario, since we simulate stall cycle based purely on the instruction number, Read1 will need to stall for 2 cycles and Read2 will need to stall for 1 cycle.
          However, after Read1 finishes stalling and starts to be processed as the "1003rd" instruction, Read2 will then be processed as the "1004th" instruction. At this stage
          it no longer needs to stall. Therefore, we should not consider Read2 as a RAW hazard nor counting its stall cycle.

          Corner case 2:

          INST_NUM    Instruction                   Readiness Table:
          1000        Load1 to R0                   R0: 1003
          1001        Load2 to R1                   R1: 1004
          1002        Read1 from R0
          1003        Read2 from R1

          In this senario, since we simulate stall cycle based purely on the instruction number, Read1 will need to stall for 1 cycle and Read2 will also need to stall for 1 cycle.
          However, after Read1 finishes stalling and starts to be processed as the "1003rd" instruction, Read2 will then be processed as the "1004th" instruction. At this stage
          it no longer needs to stall. Therefore, we should not consider Read2 as a RAW hazard nor counting its stall cycle.


          In conclusion, to address the two corner case stated above, we shift the instruction number for all registers in the readiness table earlier by the number of stall cycles
          required by the current instruction being examined. This effectively counts the stalling time of the current instruction towards the time that future instructions wait for
          the registers they depend on to be ready.
          */
          for(int i = 0; i<MD_TOTAL_REGS; i++){
            reg_ready_q2[i] -= max_stall;
          }
        }

        if (r_out[0] != DNA){
          // Load
          if(MD_OP_FLAGS(op) & F_MEM && MD_OP_FLAGS(op) & F_LOAD){
            reg_ready_q2[r_out[0]] = sim_num_insn + 3;
          }
          // Write
          else{
            reg_ready_q2[r_out[0]] = sim_num_insn + 2;
          }
        }

        if (r_out[1] != DNA){
          // Load
          if(MD_OP_FLAGS(op) & F_MEM && MD_OP_FLAGS(op) & F_LOAD){
            reg_ready_q2[r_out[1]] = sim_num_insn + 3;
          }
          // Write
          else{
            reg_ready_q2[r_out[1]] = sim_num_insn + 2;
          }
        }
      }
      /* ECE552 Assignment 1 - END CODE*/

      /* check for DLite debugger entry condition */
      if (dlite_check_break(regs.regs_NPC,
			    is_write ? ACCESS_WRITE : ACCESS_READ,
			    addr, sim_num_insn, sim_num_insn))
	dlite_main(regs.regs_PC, regs.regs_NPC, sim_num_insn, &regs, mem);

      /* go to the next instruction */
      regs.regs_PC = regs.regs_NPC;
      regs.regs_NPC += sizeof(md_inst_t);

      /* finish early? */
      if (max_insts && sim_num_insn >= max_insts)
	return;
    }
}
