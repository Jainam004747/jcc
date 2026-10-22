// jcc: a small C compiler that turns a useful subset of C into x86-64 assembly.
//
// The pipeline has four stages, one file each:
//
//   source text --tokenize.c--> tokens --parse.c--> AST --type.c--> typed AST --codegen.c--> assembly
//
// This header holds every shared data structure, so it is the best place to start reading.

#ifndef JCC_H
#define JCC_H

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct Type Type;
typedef struct Node Node;
typedef struct Member Member;

// ---------------------------------------------------------------------------
// tokenize.c: source text -> linked list of tokens
// ---------------------------------------------------------------------------

typedef enum {
  TK_IDENT,   // names: main, x, printf
  TK_PUNCT,   // punctuators: + - == ( { ...
  TK_KEYWORD, // reserved words: int, return, while, ...
  TK_STR,     // string literal "hello"
  TK_NUM,     // integer or character literal: 42, 0x1F, 'a'
  TK_EOF,     // end of input
} TokenKind;

typedef struct Token Token;
struct Token {
  TokenKind kind;
  Token *next;
  int64_t val;   // TK_NUM: value
  char *loc;     // where the token starts in the source buffer
  int len;       // length in the source
  Type *ty;      // TK_STR: type (array of char)
  char *str;     // TK_STR: contents with escape sequences decoded
  int line_no;   // for error messages
};

void error(char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void error_at(char *loc, char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void error_tok(Token *tok, char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
void warn_tok(Token *tok, char *fmt, ...) __attribute__((format(printf, 2, 3)));
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *tokenize(char *filename, char *input);
char *read_file(char *path);
char *format(char *fmt, ...) __attribute__((format(printf, 1, 2)));

// ---------------------------------------------------------------------------
// parse.c: tokens -> abstract syntax tree (AST)
// ---------------------------------------------------------------------------

// A variable or a function. Locals live on the stack; globals and functions are symbols.
typedef struct Obj Obj;
struct Obj {
  Obj *next;
  char *name;
  Type *ty;
  Token *tok;     // where it was declared, for error messages
  bool is_local;

  // Local variable: offset from %rbp (negative)
  int offset;

  // Global variable or function
  bool is_function;
  bool is_definition;

  // Global variable initial value: raw bytes plus relocations (pointers to other symbols)
  char *init_data;
  struct Relocation *rel;

  // Function
  Obj *params;
  Node *body;
  Obj *locals;
  int stack_size;
};

// A pointer stored in a global's initial data, e.g. char *msg = "hi"; needs ".quad .L.str.3"
typedef struct Relocation Relocation;
struct Relocation {
  Relocation *next;
  int offset;
  char *label;
  long addend;
};

typedef enum {
  ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_MOD,       // + - * / %
  ND_BITAND, ND_BITOR, ND_BITXOR, ND_SHL, ND_SHR, // & | ^ << >>
  ND_EQ, ND_NE, ND_LT, ND_LE,                   // == != < <=  (> and >= are swapped into < <=)
  ND_NEG, ND_NOT, ND_BITNOT,                    // unary - ! ~
  ND_LOGAND, ND_LOGOR,                          // && ||
  ND_ASSIGN,                                    // =
  ND_COND,                                      // a ? b : c
  ND_COMMA,                                     // a, b
  ND_MEMBER,                                    // s.x (s->x is (*s).x)
  ND_ADDR,                                      // &x
  ND_DEREF,                                     // *p
  ND_FUNCALL,                                   // f(a, b)
  ND_VAR,                                       // a variable
  ND_NUM,                                       // an integer constant
  ND_CAST,                                      // (int)x and implicit conversions
  ND_MEMZERO,                                   // zero a local's memory (array/struct initializers)
  ND_RETURN, ND_IF, ND_FOR, ND_DO, ND_SWITCH, ND_CASE,
  ND_BLOCK, ND_BREAK, ND_CONTINUE, ND_EXPR_STMT,
} NodeKind;

struct Node {
  NodeKind kind;
  Node *next;   // next statement / next argument
  Type *ty;     // type of the value, filled in by add_type()
  Token *tok;   // representative token, for error messages

  Node *lhs;
  Node *rhs;

  // if / for / while / do / ?: / switch
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;

  // Labels for break / continue (loops and switch)
  char *brk_label;
  char *cont_label;

  // Block or statement-expression body
  Node *body;

  // switch: list of cases; case: its value and label
  Node *case_next;
  Node *default_case;
  long case_val;
  char *label;

  // Struct member access
  Member *member;

  // Function call
  char *funcname;
  Type *func_ty;
  Node *args;

  Obj *var;     // ND_VAR
  int64_t val;  // ND_NUM
};

Node *new_cast(Node *expr, Type *ty);
int64_t const_expr(Token **rest, Token *tok);
Obj *parse(Token *tok);

// ---------------------------------------------------------------------------
// type.c: the C type system
// ---------------------------------------------------------------------------

typedef enum {
  TY_VOID, TY_CHAR, TY_SHORT, TY_INT, TY_LONG,
  TY_PTR, TY_FUNC, TY_ARRAY, TY_STRUCT,
} TypeKind;

struct Type {
  TypeKind kind;
  int size;   // sizeof
  int align;  // alignment in bytes

  Type *base;      // pointer: pointed-to type; array: element type
  Token *name;     // declaration name, set by the declarator
  int array_len;   // array: number of elements

  Member *members; // struct
  bool is_complete;

  Type *return_ty; // function
  Type *params;
  bool is_variadic;
  Type *next;      // next parameter
};

struct Member {
  Member *next;
  Type *ty;
  Token *name;
  int offset;
};

extern Type *ty_void;
extern Type *ty_char;
extern Type *ty_short;
extern Type *ty_int;
extern Type *ty_long;

bool is_integer(Type *ty);
bool is_pointer_like(Type *ty);
Type *copy_type(Type *ty);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);
Type *array_of(Type *base, int size);
Type *struct_type(void);
void add_type(Node *node);
int align_to(int n, int align);

// ---------------------------------------------------------------------------
// codegen.c: AST -> x86-64 assembly (AT&T syntax, System V ABI)
// ---------------------------------------------------------------------------

void codegen(Obj *prog, FILE *out, char *source_path);

#endif
