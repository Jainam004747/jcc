#include <stdio.h>
int hanoi_moves;
void hanoi(int n, int from, int to, int via) {
  if (n == 0) return;
  hanoi(n - 1, from, via, to);
  hanoi_moves++;
  hanoi(n - 1, via, to, from);
}
long fib_memo[91];
long fib(int n) {
  if (n < 2) return n;
  if (fib_memo[n]) return fib_memo[n];
  return fib_memo[n] = fib(n - 1) + fib(n - 2);
}
int gcd(int a, int b) { return b == 0 ? a : gcd(b, a % b); }
int main() {
  hanoi(16, 1, 3, 2);
  printf("%d %ld %d\n", hanoi_moves, fib(90), gcd(1071, 462));
  return 0;
}
