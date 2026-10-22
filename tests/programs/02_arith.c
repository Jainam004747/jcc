#include <stdio.h>
int main() {
  printf("%d %d %d %d %d\n", 1 + 2 * 3, (1 + 2) * 3, 17 / 5, 17 % 5, -17 / 5);
  printf("%d %d %d\n", -17 % 5, 10 - 20, -(-3));
  printf("%d %d %d %d\n", 2147483647 + 1, -2147483647 - 1, 46341 * 46341, 7 * -6);
  long big = 3000000000;
  printf("%ld %ld %ld\n", big * 3, big / 7, big % 1000007);
  printf("%d %d %d %d %d\n", 6 & 3, 6 | 3, 6 ^ 3, ~5, 1 << 10);
  printf("%d %d %ld\n", -64 >> 3, 255 >> 4, 1L << 40);
  return 0;
}
