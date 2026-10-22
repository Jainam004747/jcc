#include <stdio.h>
int cols[12];
int ok(int row, int col) {
  for (int r = 0; r < row; r++) {
    int c = cols[r];
    if (c == col || c - col == r - row || c - col == row - r) return 0;
  }
  return 1;
}
int solve(int row, int n) {
  if (row == n) return 1;
  int count = 0;
  for (int c = 0; c < n; c++)
    if (ok(row, c)) { cols[row] = c; count += solve(row + 1, n); }
  return count;
}
int main() {
  for (int n = 1; n <= 8; n++) printf("%d:%d ", n, solve(0, n));
  printf("\n");
  return 0;
}
