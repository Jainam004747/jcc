// Tokenizer (lexer): splits source text into tokens.
//
// "int x = 42;" becomes:  [KEYWORD int] [IDENT x] [PUNCT =] [NUM 42] [PUNCT ;] [EOF]
//
// The parser never looks at raw characters again; it only walks this list. Each token
// remembers where it came from so error messages can point at the exact column.

#include "jcc.h"

static char *current_file;   // file name, for error messages
static char *current_input;  // whole source text

// ---- error reporting ------------------------------------------------------

// Prints "file:line:col: message" followed by the source line and a caret:
//
//   test.c:3:11: error: expected ';'
//       x = 1 + 2
//                ^
static void verror_at(char *kind, int line_no, char *loc, char *fmt, va_list ap) {
  char *line = loc;
  while (current_input < line && line[-1] != '\n')
    line--;
  char *end = loc;
  while (*end && *end != '\n')
    end++;
  int col = (int)(loc - line) + 1;
  fprintf(stderr, "%s:%d:%d: %s: ", current_file, line_no, col, kind);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n    %.*s\n", (int)(end - line), line);
  fprintf(stderr, "    %*s^\n", (int)(loc - line), "");
}

static int line_of(char *loc) {
  int n = 1;
  for (char *p = current_input; p < loc; p++)
    if (*p == '\n')
      n++;
  return n;
}

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "jcc: error: ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at("error", line_of(loc), loc, fmt, ap);
  exit(1);
}

void error_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at("error", tok->line_no, tok->loc, fmt, ap);
  exit(1);
}

void warn_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at("warning", tok->line_no, tok->loc, fmt, ap);
  va_end(ap);
}

char *format(char *fmt, ...) {
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);
  fclose(out);
  return buf;
}

// ---- helpers used by the parser -------------------------------------------

bool equal(Token *tok, char *op) {
  return (tok->kind == TK_PUNCT || tok->kind == TK_KEYWORD || tok->kind == TK_IDENT) &&
         (int)strlen(op) == tok->len && memcmp(tok->loc, op, tok->len) == 0;
}

// Requires the token to be `op` and returns the next one.
Token *skip(Token *tok, char *op) {
  if (!equal(tok, op))
    error_tok(tok, "expected '%s'", op);
  return tok->next;
}

// If the token is `str`, moves past it and returns true.
bool consume(Token **rest, Token *tok, char *str) {
  if (equal(tok, str)) {
    *rest = tok->next;
    return true;
  }
  *rest = tok;
  return false;
}

static Token *new_token(TokenKind kind, char *start, char *end) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->loc = start;
  tok->len = (int)(end - start);
  return tok;
}

static bool startswith(char *p, char *q) {
  return strncmp(p, q, strlen(q)) == 0;
}

static bool is_ident1(char c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_ident2(char c) {
  return is_ident1(c) || ('0' <= c && c <= '9');
}

// Longest punctuator first, so "<<=" is not read as "<" "<=".
static int read_punct(char *p) {
  static char *kw[] = {
    "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=", "%=",
    "&=", "|=", "^=", "++", "--", "&&", "||", "<<", ">>",
  };
  for (size_t i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (startswith(p, kw[i]))
      return (int)strlen(kw[i]);
  return strchr("+-*/%()<>{}[];=,.&|^!~?:", *p) && *p ? 1 : 0;
}

static bool is_keyword(Token *tok) {
  static char *kw[] = {
    "return", "if", "else", "for", "while", "do", "int", "char", "short", "long", "void",
    "sizeof", "struct", "typedef", "break", "continue", "switch", "case", "default", "const",
    "static", "extern", "unsigned", "signed", "float", "double", "union", "enum", "goto",
  };
  for (size_t i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (tok->len == (int)strlen(kw[i]) && memcmp(tok->loc, kw[i], tok->len) == 0)
      return true;
  return false;
}

static int from_hex(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  return c - 'A' + 10;
}

// Decodes one escape sequence after a backslash: \n \t \0 \x41 \101 \\ \' \"
static int read_escaped_char(char **new_pos, char *p) {
  if ('0' <= *p && *p <= '7') {
    int c = *p++ - '0';
    if ('0' <= *p && *p <= '7') {
      c = (c << 3) + (*p++ - '0');
      if ('0' <= *p && *p <= '7')
        c = (c << 3) + (*p++ - '0');
    }
    *new_pos = p;
    return c;
  }
  if (*p == 'x') {
    p++;
    if (!isxdigit((unsigned char)*p))
      error_at(p, "invalid hex escape sequence");
    int c = 0;
    for (; isxdigit((unsigned char)*p); p++)
      c = (c << 4) + from_hex(*p);
    *new_pos = p;
    return c;
  }
  *new_pos = p + 1;
  switch (*p) {
  case 'a': return '\a';
  case 'b': return '\b';
  case 't': return '\t';
  case 'n': return '\n';
  case 'v': return '\v';
  case 'f': return '\f';
  case 'r': return '\r';
  case 'e': return 27;
  default: return *p; // \\ \' \" \? and anything else: the character itself
  }
}

static char *string_literal_end(char *p) {
  char *start = p;
  for (; *p != '"'; p++) {
    if (*p == '\n' || *p == '\0')
      error_at(start, "unclosed string literal");
    if (*p == '\\')
      p++;
  }
  return p;
}

static Token *read_string_literal(char *start) {
  char *end = string_literal_end(start + 1);
  char *buf = calloc(1, end - start);
  int len = 0;
  for (char *p = start + 1; p < end;) {
    if (*p == '\\')
      buf[len++] = (char)read_escaped_char(&p, p + 1);
    else
      buf[len++] = *p++;
  }
  Token *tok = new_token(TK_STR, start, end + 1);
  tok->ty = array_of(ty_char, len + 1); // +1 for the terminating '\0'
  tok->str = buf;
  return tok;
}

static Token *read_char_literal(char *start) {
  char *p = start + 1;
  if (*p == '\0' || *p == '\n')
    error_at(start, "unclosed character literal");
  int c;
  if (*p == '\\')
    c = read_escaped_char(&p, p + 1);
  else
    c = (unsigned char)*p++;
  if (*p != '\'')
    error_at(p, "character literal must be a single character");
  Token *tok = new_token(TK_NUM, start, p + 1);
  tok->val = (char)c; // char is signed, like gcc on x86-64
  return tok;
}

static Token *read_number(char *start) {
  char *p = start;
  int base = 10;
  if (!strncasecmp(p, "0x", 2) && isxdigit((unsigned char)p[2])) {
    p += 2;
    base = 16;
  } else if (!strncasecmp(p, "0b", 2) && (p[2] == '0' || p[2] == '1')) {
    p += 2;
    base = 2;
  } else if (*p == '0') {
    base = 8;
  }
  int64_t val = (int64_t)strtoul(p, &p, base);
  // Suffixes: 10L / 10LL make the constant a long; U is accepted but jcc has no unsigned types.
  bool is_long = false;
  while (*p == 'l' || *p == 'L' || *p == 'u' || *p == 'U') {
    if (*p == 'l' || *p == 'L')
      is_long = true;
    p++;
  }
  if (is_ident2(*p))
    error_at(p, "invalid digit in number");
  Token *tok = new_token(TK_NUM, start, p);
  tok->val = val;
  if (is_long)
    tok->ty = ty_long;
  return tok;
}

// Assigns line numbers to all tokens in one pass (cheaper than counting per token).
static void add_line_numbers(Token *tok, char *input) {
  char *p = input;
  int n = 1;
  do {
    if (p == tok->loc) {
      tok->line_no = n;
      tok = tok->next;
    }
    if (*p == '\n')
      n++;
  } while (*p++);
}

Token *tokenize(char *filename, char *p) {
  current_file = filename;
  current_input = p;
  Token head = {0};
  Token *cur = &head;
  bool at_line_start = true;

  while (*p) {
    // Preprocessor lines. jcc has no preprocessor: #include lines are skipped because a
    // built-in prelude already declares common C library functions (see main.c).
    if (at_line_start && *p == '#') {
      char *start = p;
      while (*p && *p != '\n')
        p++;
      if (strncmp(start, "#include", 8) != 0 && strncmp(start, "#pragma", 7) != 0)
        error_at(start, "preprocessor directives other than #include are not supported");
      continue;
    }
    if (*p == '\n') {
      at_line_start = true;
      p++;
      continue;
    }
    if (isspace((unsigned char)*p)) {
      p++;
      continue;
    }
    at_line_start = false;

    if (startswith(p, "//")) {
      p += 2;
      while (*p && *p != '\n')
        p++;
      continue;
    }
    if (startswith(p, "/*")) {
      char *q = strstr(p + 2, "*/");
      if (!q)
        error_at(p, "unclosed block comment");
      p = q + 2;
      continue;
    }
    if (isdigit((unsigned char)*p)) {
      cur = cur->next = read_number(p);
      p += cur->len;
      continue;
    }
    if (*p == '"') {
      cur = cur->next = read_string_literal(p);
      p += cur->len;
      continue;
    }
    if (*p == '\'') {
      cur = cur->next = read_char_literal(p);
      p += cur->len;
      continue;
    }
    if (is_ident1(*p)) {
      char *start = p;
      do {
        p++;
      } while (is_ident2(*p));
      cur = cur->next = new_token(TK_IDENT, start, p);
      if (is_keyword(cur))
        cur->kind = TK_KEYWORD;
      continue;
    }
    int punct_len = read_punct(p);
    if (punct_len) {
      cur = cur->next = new_token(TK_PUNCT, p, p + punct_len);
      p += punct_len;
      continue;
    }
    error_at(p, "invalid character '%c'", *p);
  }
  cur = cur->next = new_token(TK_EOF, p, p);
  add_line_numbers(head.next, current_input);
  return head.next;
}

char *read_file(char *path) {
  FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
  if (!fp)
    error("cannot open %s: %s", path, strerror(errno));
  char *buf;
  size_t buflen;
  FILE *out = open_memstream(&buf, &buflen);
  char chunk[4096];
  size_t n;
  while ((n = fread(chunk, 1, sizeof(chunk), fp)) > 0)
    fwrite(chunk, 1, n, out);
  if (fp != stdin)
    fclose(fp);
  // Make sure the file ends with a newline, then a NUL terminator.
  fflush(out);
  if (buflen == 0 || buf[buflen - 1] != '\n')
    fputc('\n', out);
  fputc('\0', out);
  fclose(out);
  return buf;
}
