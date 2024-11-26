#include <stdio.h>
#include <stdlib.h>

// /cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq1.c -o mbq1_assembly -O0 -S <- this generate assembly
// view by directly opening the generated benchmark_assembly file, objdump DOES NOT WORK on this
// /cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq1.c -o mbq1 -O0 <- this is the exe feed into sim-safe
// only viewable of the assembly with objdump, but this is what is passed with sim-safe
// ./sim-safe mbq1

// from running sim-safe, this is result of benchmark:
// sim_num_RAW_hazard_q1       2000939 # total number of RAW hazards (q1)
// sim_num_RAW_hazard_q1_1stall      1000080 # total number of 1 cycle stall RAW hazards (q1)
// sim_num_RAW_hazard_q1_2stall      1000859 # total number of 2 cycle stall RAW hazards (q1)
// CPI_from_RAW_hazard_q1       1.2308 # CPI from RAW hazard (q1)
// expected # of RAW hazard for 1 stall is ~1M, 2 stall is also ~1M, therefore our benchmark shows
// that the sim-safe is working correctly
int main (){
    // NOTE: MUST USE register keyword to force compiler to store values in register
    // rather than optimize away and store in memory
    // you see these inst. instead of some sw inst.
    // move    $3,$0
    // move    $4,$0
    // move    $5,$0
    // move    $6,$0
    // move    $7,$0
    // move    $8,$0
    // li    $9,0x00000001        # 1
    // li    $10,0x000f4240        # 1000000
    register int a = 0;
    register int b = 0;
    register int c = 0;
    register int d = 0;
    register int i = 0;
    register int random_inst = 0;
    register int loop_cond = 1;
    // 1M loop to minimize impact of potential stalls during init stages
    register int loop = 1000000;

    // using loop this way to avoid encountering compiler making ton of branching stalls
    while (loop_cond){
        // RAW 1 cycle stall
        // in assembly, it assigns R3 to be 1, then random inst., then read, causing 1 cycle stall
        // li    $3,0x00000001        # 1
        // move    $8,$0
        // move    $4,$3
        a = 1;
        // extra inst to make it a 1 cycle stall
        random_inst = 0;
        b = a;
        i++;
        // extra inst to prevent stalling due to i
        random_inst = 0;
        random_inst = 0;
        // RAW 2 cycle stall
        // straight read R5 after writing to R5, 2 cycle stalls
        // li    $5,0x00000001        # 1
        // move    $6,$5
        c = 1;
        d = c;
        loop_cond = i < loop;
        // extra inst to prevent stalling due to loop_cond
        random_inst = 0;
        random_inst = 0;
    }
    return 0;
}