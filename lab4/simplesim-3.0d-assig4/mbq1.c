// compilation flags:
// /cad2/ece552f/compiler/bin/ssbig-na-sstrix-gcc mbq1.c -o mbq1 -O0

#define ARRAY_SIZE 1000000
#define BLOCK_SIZE 64

int main() {
    // memory access
    char array[ARRAY_SIZE];
    int i = 0;
    register char data = 'a';

    // This should result in near 100% prefetch hit
    // Program begins with loads array[0-63] into the cache block (initially miss)
    // Using nextline prefetch load array[64-127] into another cache block
    // This is maximum jump that the program can do and still result in near 100% hit rate
    // if this value goes beyond BLOCK_SIZE, then we observe ton of cache miss, as expected
    while (i < ARRAY_SIZE){
        array[i] = data;
        i += BLOCK_SIZE;
    }

    return 0;
}