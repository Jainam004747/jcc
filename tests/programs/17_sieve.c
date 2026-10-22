#include <stdio.h>
char composite[10001];
int main() {
  int count = 0;
  long sum = 0;
  for (int i = 2; i <= 10000; i++) {
    if (composite[i]) continue;
    count++;
    sum += i;
    for (int j = i * i; j <= 10000; j += i) composite[j] = 1;
  }
  printf("%d primes, sum %ld\n", count, sum);
  return 0;
}
