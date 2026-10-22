#include <stdio.h>
// A tiny recursive-descent calculator: a compiler front end inside a program compiled by jcc.
char *p;
int expr();
int number() { int n = 0; while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0'); return n; }
int primary() {
  if (*p == '(') { p++; int v = expr(); p++; return v; }
  if (*p == '-') { p++; return -primary(); }
  return number();
}
int term() {
  int v = primary();
  while (*p == '*' || *p == '/') { char op = *p++; int r = primary(); v = op == '*' ? v * r : v / r; }
  return v;
}
int expr() {
  int v = term();
  while (*p == '+' || *p == '-') { char op = *p++; int r = term(); v = op == '+' ? v + r : v - r; }
  return v;
}
int eval(char *s) { p = s; return expr(); }
int main() {
  char *tests[] = {"1+2*3", "(1+2)*3", "100/7-2", "-(4+4)*2", "2*(3+(4-1))*5"};
  for (int i = 0; i < 5; i++) printf("%s = %d\n", tests[i], eval(tests[i]));
  return 0;
}
