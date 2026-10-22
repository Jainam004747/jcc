#include <stdio.h>
int add6(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
int fact(int n) { if (n <= 1) return 1; return n * fact(n - 1); }
int is_even(int n);
int is_odd(int n) { return n == 0 ? 0 : is_even(n - 1); }
int is_even(int n) { return n == 0 ? 1 : is_odd(n - 1); }
void hello(void) { printf("hello from void\n"); }
long sq(long x) { return x * x; }
char next_char(char c) { return c + 1; }
int ackermann(int m, int n) {
  if (m == 0) return n + 1;
  if (n == 0) return ackermann(m - 1, 1);
  return ackermann(m - 1, ackermann(m, n - 1));
}
int main() {
  printf("%d %d %d %d\n", add6(1, 2, 3, 4, 5, 6), fact(10), is_even(10), is_odd(7));
  hello();
  printf("%ld %c %d\n", sq(100000), next_char('a'), ackermann(2, 3));
  printf("%d %d %d %d %d %d %d %d %d\n", 1, 2, 3, 4, 5, 6, 7, 8, 9);
  return 0;
}
