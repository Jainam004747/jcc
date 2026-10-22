#include <stdio.h>
int counts[26];
int main() {
  char *text = "the quick brown fox jumps over the lazy dog, then jcc compiles it";
  for (char *c = text; *c; c++)
    if (*c >= 'a' && *c <= 'z') counts[*c - 'a']++;
  for (int i = 0; i < 26; i++)
    if (counts[i] > 2) printf("%c=%d ", 'a' + i, counts[i]);
  printf("\n");
  return 0;
}
