#include <stdio.h>
#include <string.h>
int my_strlen(char *s) { int n = 0; while (*s++) n++; return n; }
void reverse(char *s) {
  int n = my_strlen(s);
  for (int i = 0; i < n / 2; i++) { char t = s[i]; s[i] = s[n - 1 - i]; s[n - 1 - i] = t; }
}
int main() {
  char buf[32];
  strcpy(buf, "compiler");
  reverse(buf);
  printf("%s %d %ld\n", buf, my_strlen(buf), strlen("hello"));
  printf("%d %d\n", strcmp("abc", "abd") < 0, strcmp("x", "x"));
  char *escapes = "tab\tnew\\n quote\" \x41\101";
  printf("%s|%d\n", escapes, '\n');
  char msg[64];
  sprintf(msg, "%d-%s", 42, "ok");
  puts(msg);
  return 0;
}
