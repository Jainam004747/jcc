#include <stdio.h>
void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }
int partition(int *a, int lo, int hi) {
  int pivot = a[hi];
  int i = lo - 1;
  for (int j = lo; j < hi; j++)
    if (a[j] <= pivot) swap(&a[++i], &a[j]);
  swap(&a[i + 1], &a[hi]);
  return i + 1;
}
void quicksort(int *a, int lo, int hi) {
  if (lo < hi) { int p = partition(a, lo, hi); quicksort(a, lo, p - 1); quicksort(a, p + 1, hi); }
}
int main() {
  int a[20];
  int seed = 12345;
  for (int i = 0; i < 20; i++) { seed = (seed * 1103515245 + 12345) % 2147483647; if (seed < 0) seed = -seed; a[i] = seed % 1000; }
  quicksort(a, 0, 19);
  for (int i = 0; i < 20; i++) printf("%d ", a[i]);
  printf("\n");
  return 0;
}
