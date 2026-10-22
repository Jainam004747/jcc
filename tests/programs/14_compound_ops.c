#include <stdio.h>
int main() {
  int a = 10;
  a += 5; a -= 3; a *= 4; a /= 6; a %= 5;
  printf("%d\n", a);
  int b = 6;
  b <<= 3; b >>= 1; b &= 30; b |= 1; b ^= 3;
  printf("%d\n", b);
  int arr[4] = {0, 0, 0, 0};
  int i = 0;
  arr[i++] += 5;
  arr[i++] += 6;
  printf("%d %d %d %d i=%d\n", arr[0], arr[1], arr[2], arr[3], i);
  int x = 5;
  int y = x++;
  y += ++x;
  printf("%d %d\n", x, y);
  int old = x--;
  int pre = --x;
  printf("%d %d %d\n", old, pre, x);
  int *p = arr;
  p += 1;
  *p *= 10;
  printf("%d\n", arr[1]);
  return 0;
}
