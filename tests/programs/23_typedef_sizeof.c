#include <stdio.h>
typedef int number;
typedef struct { number x; number y; } vec2;
typedef vec2 *vec2_ptr;
int main() {
  vec2 v = {3, 4};
  vec2_ptr pv = &v;
  number n = pv->x * pv->x + pv->y * pv->y;
  printf("%d %ld %ld %ld\n", n, sizeof(vec2), sizeof(vec2_ptr), sizeof(number));
  int calls = 0;
  long sz = sizeof(calls++);   // sizeof never evaluates its operand
  printf("%ld %d %ld %ld\n", sz, calls, sizeof(int *), sizeof(char[10]));
  return 0;
}
