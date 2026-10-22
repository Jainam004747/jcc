#include <stdio.h>
void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }
int main() {
  int x = 3, y = 4;
  swap(&x, &y);
  printf("%d %d\n", x, y);
  int *p = &x;
  *p = 10;
  int **pp = &p;
  **pp += 5;
  printf("%d\n", x);
  int a[5] = {10, 20, 30, 40, 50};
  int *q = a;
  printf("%d %d\n", *(q + 2), q[4]);
  printf("%d\n", *q++);
  printf("%d %ld\n", *q, &a[4] - &a[1]);
  q += 2;
  printf("%d\n", *q);
  char *s = "abc";
  printf("%c%c%c\n", s[2], *(s + 1), *s);
  void *v = a;
  int *back = v;
  printf("%d\n", back[1]);
  return 0;
}
