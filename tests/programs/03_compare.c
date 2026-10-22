#include <stdio.h>
int main() {
  printf("%d %d %d %d %d %d\n", 1 < 2, 2 < 1, 2 <= 2, 3 > 2, 2 >= 3, 5 == 5);
  printf("%d %d %d %d\n", 5 != 5, !0, !7, -1 < 0);
  printf("%d %d %d %d\n", 1 && 2, 1 && 0, 0 || 3, 0 || 0);
  int calls = 0;
  int r = 0 && (calls = 1);
  r = 1 || (calls = 2);
  printf("short-circuit calls=%d r=%d\n", calls, r);
  printf("%d %d\n", 3 > 2 ? 10 : 20, 0 ? 1 : 2 ? 3 : 4);
  return 0;
}
