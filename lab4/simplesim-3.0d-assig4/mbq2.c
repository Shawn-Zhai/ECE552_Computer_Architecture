// compilation flags:
// /cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq2.c -o mbq2 -O0

#define ARRAY_SIZE 1000000
#define STRIDE 128         // Stride of 128 bytes (2 cache blocks of 64 bytes each)
#define JUMPS 10           // Number of memory jumps in each iteration

int main() {
    int array[ARRAY_SIZE];
    register int i, j;

    // Access the array with repeated jumps
    // should cause ton of data miss if using nextline prefetcher
    // but very little cache data miss if using stride prefetcher
    for (i = 0; i < ARRAY_SIZE; i += STRIDE) {
        for (j = 0; j < JUMPS; j++) {
            int idx = (i + j * STRIDE / sizeof(int)) % ARRAY_SIZE; // Simulate jumps
            array[idx] = idx; // Simulate memory access
        }
    }

    return 0;
}