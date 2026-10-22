// Pointers: a pointer is a variable that holds a memory address.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// Changing a caller's variables needs their addresses: C passes everything by value.
static void swap(int *a, int *b) {
  int tmp = *a; // *a means "the int stored at address a"
  *a = *b;
  *b = tmp;
}

// An array argument is really a pointer to its first element, so its length must be passed.
static int sum(const int *values, int n) {
  int total = 0;
  for (const int *p = values; p < values + n; p++) // p + 1 moves by sizeof(int) bytes
    total += *p;
  return total;
}

// Returning a pointer to a LOCAL variable is a classic bug: the stack frame is gone after return.
// Heap memory from malloc lives until free(), so it can be returned.
static int *make_counter(int start) {
  int *c = malloc(sizeof *c);
  assert(c != NULL);
  *c = start;
  return c;
}

int main(void) {
  int x = 1, y = 2;
  swap(&x, &y);
  assert(x == 2 && y == 1);

  int nums[4] = {10, 20, 30, 40};
  assert(sum(nums, 4) == 100);
  assert(*(nums + 2) == nums[2]); // a[i] is defined as *(a + i)
  assert(&nums[3] - &nums[0] == 3); // pointer difference counts elements, not bytes

  int *counter = make_counter(41);
  (*counter)++;
  assert(*counter == 42);
  free(counter);

  printf("01_pointers: ok (int is %zu bytes, a pointer is %zu bytes)\n", sizeof(int), sizeof(int *));
  return 0;
}
