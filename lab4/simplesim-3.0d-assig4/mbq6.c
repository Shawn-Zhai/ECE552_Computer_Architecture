#define ARRAY_SIZE 100000
#define STRIDE 128
#define ITERATIONS 100

// test depth: our open end work by prefetching addr + stride
// and addr + 2*stride together
int main() {
  int array[ARRAY_SIZE];
  register int a = 0;
  register int index1, index2;     // Two indices for memory accesses

  // Memory access pattern: two addresses per iteration
  while (a < ITERATIONS) {
      // First access: addr + stride
      index1 = a * (STRIDE / 4);
      array[index1 % ARRAY_SIZE] = a;

      // Second access: addr + 2 * stride
      index2 = index1 + (STRIDE / 4);
      array[index1 % ARRAY_SIZE] = a;
      a++;
  }

  return 0;
}