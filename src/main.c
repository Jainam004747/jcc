// jcc driver: reads a C file, runs the compiler stages, and optionally calls the system
// assembler and linker (through `cc`) to produce an executable.
//
//   jcc -S prog.c -o prog.s     compile to assembly only
//   jcc prog.c -o prog          compile, assemble and link
//   jcc --dump-tokens prog.c    show the tokenizer's output (handy for learning)

#include "jcc.h"

#include <sys/wait.h>
#include <unistd.h>

// jcc has no preprocessor, so it cannot read the system's <stdio.h>. Instead this prelude
// declares the C library functions test programs commonly use. `long` stands in for size_t.
static char *prelude =
  "int printf(char *fmt, ...);\n"
  "int sprintf(char *buf, char *fmt, ...);\n"
  "int snprintf(char *buf, long n, char *fmt, ...);\n"
  "int puts(char *s);\n"
  "int putchar(int c);\n"
  "int getchar(void);\n"
  "void *malloc(long size);\n"
  "void *calloc(long n, long size);\n"
  "void *realloc(void *p, long size);\n"
  "void free(void *p);\n"
  "void exit(int code);\n"
  "int abs(int x);\n"
  "int atoi(char *s);\n"
  "long strtol(char *s, char **end, int base);\n"
  "long strlen(char *s);\n"
  "int strcmp(char *a, char *b);\n"
  "int strncmp(char *a, char *b, long n);\n"
  "char *strcpy(char *dst, char *src);\n"
  "char *strcat(char *dst, char *src);\n"
  "char *strchr(char *s, int c);\n"
  "void *memcpy(void *dst, void *src, long n);\n"
  "void *memset(void *p, int c, long n);\n"
  "int memcmp(void *a, void *b, long n);\n";

static void usage(int status) {
  fprintf(stderr,
          "usage: jcc [-S] [-o <output>] [--dump-tokens] <file.c>\n"
          "  -S              stop after generating assembly\n"
          "  -o <output>     output file (default: a.out, or stdout with -S)\n"
          "  --dump-tokens   print the token list and exit\n");
  exit(status);
}

static void dump_tokens(Token *tok) {
  static char *names[] = {"IDENT", "PUNCT", "KEYWORD", "STR", "NUM", "EOF"};
  for (; tok->kind != TK_EOF; tok = tok->next) {
    printf("%4d  %-8s %.*s", tok->line_no, names[tok->kind], tok->len, tok->loc);
    if (tok->kind == TK_NUM)
      printf("   (value %ld)", tok->val);
    printf("\n");
  }
}

static void run(char **argv) {
  pid_t pid = fork();
  if (pid == 0) {
    execvp(argv[0], argv);
    fprintf(stderr, "jcc: cannot run %s: %s\n", argv[0], strerror(errno));
    _exit(1);
  }
  int status;
  while (waitpid(pid, &status, 0) > 0)
    ;
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    exit(1);
}

int main(int argc, char **argv) {
  char *input = NULL;
  char *output = NULL;
  bool only_asm = false;
  bool tokens_only = false;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
      usage(0);
    if (!strcmp(argv[i], "-S")) {
      only_asm = true;
      continue;
    }
    if (!strcmp(argv[i], "--dump-tokens")) {
      tokens_only = true;
      continue;
    }
    if (!strcmp(argv[i], "-o")) {
      if (++i >= argc)
        usage(1);
      output = argv[i];
      continue;
    }
    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      fprintf(stderr, "jcc: unknown option %s\n", argv[i]);
      usage(1);
    }
    input = argv[i];
  }
  if (!input)
    usage(1);

  // Stage 1: tokenize the prelude and the user's file, and join the two token lists.
  Token *pre = tokenize("<prelude>", prelude);
  Token *last = pre;
  while (last->next->kind != TK_EOF)
    last = last->next;
  Token *user = tokenize(input, read_file(input));
  if (tokens_only) {
    dump_tokens(user);
    return 0;
  }
  last->next = user;

  // Stage 2 + 3: parse into an AST (types are added while parsing).
  Obj *prog = parse(pre);

  // Stage 4: generate assembly.
  if (only_asm) {
    FILE *out = stdout;
    if (output && strcmp(output, "-")) {
      out = fopen(output, "w");
      if (!out)
        error("cannot open %s: %s", output, strerror(errno));
    }
    codegen(prog, out, input);
    if (out != stdout)
      fclose(out);
    return 0;
  }

  // Full build: write assembly to a temp file, then let `cc` assemble and link it with libc.
  char asm_path[] = "/tmp/jcc-XXXXXX.s";
  int fd = mkstemps(asm_path, 2);
  if (fd < 0)
    error("cannot create temporary file: %s", strerror(errno));
  FILE *out = fdopen(fd, "w");
  codegen(prog, out, input);
  fclose(out);
  char *cc_argv[] = {"cc", "-o", output ? output : "a.out", asm_path, NULL};
  run(cc_argv);
  unlink(asm_path);
  return 0;
}
