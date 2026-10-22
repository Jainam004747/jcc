#include <stdio.h>
long weighted(long a, long b, long c, long d, long e, long f) { return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f; }
int main() {
  printf("%ld\n", weighted(1, 2, 3, 4, 5, 6));
  printf("%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s\n", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, "end");
  printf("%s %s %s %s %s %s %s %s\n", "a", "b", "c", "d", "e", "f", "g", "h");
  return 0;
}
