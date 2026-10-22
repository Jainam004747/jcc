#include <stdio.h>
// A Brainfuck interpreter: a small program that runs another program.
char tape[1000];
int main() {
  char *prog = "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.";
  char *ip = prog;
  char *dp = tape;
  while (*ip) {
    switch (*ip) {
    case '>': dp++; break;
    case '<': dp--; break;
    case '+': (*dp)++; break;
    case '-': (*dp)--; break;
    case '.': putchar(*dp); break;
    case '[':
      if (!*dp) { int depth = 1; while (depth) { ip++; if (*ip == '[') depth++; if (*ip == ']') depth--; } }
      break;
    case ']':
      if (*dp) { int depth = 1; while (depth) { ip--; if (*ip == ']') depth++; if (*ip == '[') depth--; } }
      break;
    }
    ip++;
  }
  return 0;
}
