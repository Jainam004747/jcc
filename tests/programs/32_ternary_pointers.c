#include <stdio.h>
int max(int *a, int n) { int *best = a; for (int i = 1; i < n; i++) best = a[i] > *best ? &a[i] : best; return *best; }
int main() {
  int a[] = {4, 9, 2, 9, 1};
  char *s = 0;
  printf("%d %s %s\n", max(a, 5), s ? s : "(null)", a[1] == a[3] ? "eq" : "ne");
  return 0;
}
