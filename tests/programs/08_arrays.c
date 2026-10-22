#include <stdio.h>
int grid[3][4];
int primes[] = {2, 3, 5, 7, 11, 13};
int sum(int *a, int n) { int s = 0; for (int i = 0; i < n; i++) s += a[i]; return s; }
int main() {
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 4; j++)
      grid[i][j] = i * 10 + j;
  printf("%d %d %ld %ld\n", grid[2][3], grid[1][0], sizeof(grid), sizeof(grid[0]));
  printf("%d %ld\n", sum(primes, 6), sizeof(primes) / sizeof(primes[0]));
  int local[2][3] = {{1, 2, 3}, {4, 5, 6}};
  printf("%d %d\n", local[1][2], local[0][1]);
  int zeros[100] = {7};
  int s = 0;
  for (int i = 0; i < 100; i++) s += zeros[i];
  printf("%d\n", s);
  char word[] = "jcc";
  printf("%s %ld\n", word, sizeof(word));
  return 0;
}
