#include <stdio.h>
int main() {
  int sum = 0;
  for (int i = 1; i <= 100; i++) sum += i;
  printf("for %d\n", sum);
  int i = 0;
  while (i < 10) i += 3;
  printf("while %d\n", i);
  int n = 0;
  do { n++; } while (n < 5);
  printf("do %d\n", n);
  int odd = 0;
  for (int k = 0; k < 20; k++) {
    if (k % 2 == 0) continue;
    if (k > 15) break;
    odd += k;
  }
  printf("odd %d\n", odd);
  int count = 0;
  for (int a = 0; a < 5; a++)
    for (int b = 0; b < 5; b++) {
      if (b == a) break;
      count++;
    }
  printf("nested %d\n", count);
  for (;;) { if (++count > 20) break; }
  printf("forever %d\n", count);
  return 0;
}
