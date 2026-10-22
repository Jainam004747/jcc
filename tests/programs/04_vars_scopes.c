#include <stdio.h>
int x = 1;
int main() {
  printf("%d\n", x);
  int x = 2;
  printf("%d\n", x);
  { int x = 3; printf("%d\n", x); { x = 4; } printf("%d\n", x); }
  printf("%d\n", x);
  int a = 1, b = 2, c;
  c = a = b = 9;
  printf("%d %d %d\n", a, b, c);
  return 0;
}
