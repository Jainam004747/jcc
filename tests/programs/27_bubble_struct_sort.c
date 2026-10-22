#include <stdio.h>
#include <string.h>
struct player { char name[12]; int score; };
int main() {
  struct player ps[5] = {{"ann", 40}, {"bob", 95}, {"cid", 70}, {"dee", 95}, {"eve", 10}};
  for (int i = 0; i < 5; i++)
    for (int j = 0; j < 4 - i; j++)
      if (ps[j].score < ps[j + 1].score) { struct player t = ps[j]; ps[j] = ps[j + 1]; ps[j + 1] = t; }
  for (int i = 0; i < 5; i++) printf("%d. %s %d\n", i + 1, ps[i].name, ps[i].score);
  return 0;
}
