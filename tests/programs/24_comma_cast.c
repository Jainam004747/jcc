#include <stdio.h>
int main() {
  int a = (1, 2, 3);
  int i, j;
  for (i = 0, j = 10; i < j; i++, j--) ;
  printf("%d %d %d\n", a, i, j);
  long l = (long)1 << 33;
  int truncated = (int)l;
  char c = (char)0x141;
  printf("%ld %d %d %d\n", l, truncated, c, (int)'A' + 1);
  void *p = &a;
  printf("%d\n", *(int *)p);
  return 0;
}
