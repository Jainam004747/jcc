#include <stdio.h>
int main() {
  char c = 127;
  c = c + 1;
  short s = 32767;
  s++;
  long l = 1;
  for (int i = 0; i < 62; i++) l *= 2;
  printf("%d %d %ld\n", c, s, l);
  char bytes[4] = {-1, 0, 1, 127};
  printf("%d %d %ld %ld %ld\n", bytes[0], bytes[3], sizeof(char), sizeof(short), sizeof(long));
  int i = (char)300;
  long big = (int)4294967297L;
  printf("%d %ld %d\n", i, big, (short)70000);
  return 0;
}
