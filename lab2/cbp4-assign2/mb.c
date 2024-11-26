#include <stdio.h>
// compile command: gcc -static -O0 -o mb mb.c
// compile for assembly: gcc -static -O0 -S -o mb_asm mb.c
// NOTE: -static otherwise benchtrace failure
// -O0 otherwise branches get optimized away

int main(){
    register int i = 0;
    register int j = 0;
    int dummy = 1;

    // expect near-perfect prediction accuracy
    // history bit = 6, pattern T T T T T T NT repeat, pattern size 7, which can be handled by 6 bits
    // assembly:
    // .L4:
    // movl$2, -20(%rbp)
    // addl$1, %r12d
    // .L3:
    // cmpl$5, %r12d
    // jle.L4 //this is the inner loop
    // addl$1, %ebx
    // .L2:
    // cmpl$999999, %ebx
    // jle.L5 //this is the outer loop

    // 2 jump instuctions are apart by 8 bits so they sit on different rows and no alias
    // run result:
    // NUM_INSTRUCTIONS         :   31045528
    // NUM_CONDITIONAL_BR       :    8011815
    // 2level:  NUM_MISPREDICTIONS      :       1324
    // 2level:  MISPRED_PER_1K_INST     :      0.043

    // 1324 <<< 1 mil, so results prove the 2-level is able to handle this case near perfect
    // for(i = 0; i < 1000000; i++){
    //     for(j = 0; j < 6; j++){
    //         dummy = 2;
    //     }
    // }

    // if we run this block, then it will be very different.
    // we expect to see a drastic jump in mispredictions because it now has a repeating pattern of 8
    // NUM_INSTRUCTIONS         :   35045528
    // NUM_CONDITIONAL_BR       :    9011815
    // 2level:  NUM_MISPREDICTIONS      :    1001323
    // 2level:  MISPRED_PER_1K_INST     :     28.572
    // this confirms our predictor is behaving exactly like we expected, so our implementation is correct
    for (i=0; i < 1000000; i++){
        for (j = 0; j < 7; j++){
            dummy = 3;
        }
    }
    return 0;
}