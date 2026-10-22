#include <stdio.h>
void matmul(int n, int a[4][4], int b[4][4], int c[4][4]) {
  for (int i = 0; i < n; i++)
    for (int j = 0; j < n; j++) {
      c[i][j] = 0;
      for (int k = 0; k < n; k++) c[i][j] += a[i][k] * b[k][j];
    }
}
int main() {
  int a[4][4], b[4][4], c[4][4];
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) { a[i][j] = i + j; b[i][j] = i == j ? 2 : 0; }
  matmul(4, a, b, c);
  for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) printf("%3d", c[i][j]); printf("\n"); }
  return 0;
}
