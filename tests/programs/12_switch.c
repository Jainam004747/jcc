#include <stdio.h>
char *name(int d) {
  switch (d) {
  case 0: return "zero";
  case 1: return "one";
  case 2:
  case 3: return "two-or-three";
  default: return "many";
  }
}
int main() {
  for (int i = 0; i < 6; i++) printf("%s ", name(i));
  printf("\n");
  int fall = 0;
  switch (2) { case 1: fall += 1; case 2: fall += 2; case 3: fall += 3; break; case 4: fall += 4; }
  printf("%d\n", fall);
  int loops = 0;
  for (int i = 0; i < 5; i++) {
    switch (i) { case 2: continue; default: break; }
    loops++;
  }
  printf("%d\n", loops);
  return 0;
}
