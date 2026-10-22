// Code generator: typed AST -> x86-64 assembly (AT&T syntax, System V calling convention).
//
// jcc uses the simplest strategy that works: a "stack machine".
//   * Every expression leaves its value in %rax.
//   * For a binary operation, the right side is computed and PUSHed, the left side is computed
//     into %rax, the right side is POPped into %rdi, and one instruction combines them.
//
//     1 + 2  =>   mov $2, %rax ; push %rax ; mov $1, %rax ; pop %rdi ; add %rdi, %rax
//
// It produces slow code (real compilers allocate registers), but every step is easy to follow.
//
// Values are always kept sign-extended to 64 bits in %rax. Arithmetic on int is done with
// 32-bit instructions (so it wraps like a real int) and then sign-extended again with cltq.

#include "jcc.h"

static FILE *output_file;
static int depth;        // values currently pushed on the stack (for call alignment)
static Obj *current_fn;

static char *argreg8[] = {"%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"};
static char *argreg16[] = {"%di", "%si", "%dx", "%cx", "%r8w", "%r9w"};
static char *argreg32[] = {"%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"};
static char *argreg64[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};

static void gen_expr(Node *node);
static void gen_stmt(Node *node);

__attribute__((format(printf, 1, 2)))
static void println(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(output_file, fmt, ap);
  va_end(ap);
  fprintf(output_file, "\n");
}

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  println("  push %%rax");
  depth++;
}

static void pop(char *arg) {
  println("  pop %s", arg);
  depth--;
}

// Computes the ADDRESS of an lvalue (something that can be assigned to) into %rax.
static void gen_addr(Node *node) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local)
      println("  lea %d(%%rbp), %%rax", node->var->offset);
    else
      println("  lea %s(%%rip), %%rax", node->var->name); // RIP-relative: works in PIE binaries
    return;
  case ND_DEREF:
    gen_expr(node->lhs); // the pointer's value IS the address
    return;
  case ND_COMMA:
    gen_expr(node->lhs);
    gen_addr(node->rhs);
    return;
  case ND_MEMBER:
    gen_addr(node->lhs);
    println("  add $%d, %%rax", node->member->offset);
    return;
  default:
    error_tok(node->tok, "not an lvalue");
  }
}

// Loads the value at the address in %rax, sign-extending it to 64 bits.
static void load(Type *ty) {
  if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_FUNC)
    return; // an array/struct "value" is its address; nothing to load
  switch (ty->size) {
  case 1: println("  movsbq (%%rax), %%rax"); return;
  case 2: println("  movswq (%%rax), %%rax"); return;
  case 4: println("  movslq (%%rax), %%rax"); return;
  default: println("  mov (%%rax), %%rax"); return;
  }
}

// Stores %rax at the address on top of the stack.
static void store(Type *ty) {
  pop("%rdi");
  if (ty->kind == TY_STRUCT) {
    // Struct assignment copies every byte: %rax = source address, %rdi = destination
    for (int i = 0; i < ty->size; i++) {
      println("  mov %d(%%rax), %%r8b", i);
      println("  mov %%r8b, %d(%%rdi)", i);
    }
    return;
  }
  switch (ty->size) {
  case 1: println("  mov %%al, (%%rdi)"); return;
  case 2: println("  mov %%ax, (%%rdi)"); return;
  case 4: println("  mov %%eax, (%%rdi)"); return;
  default: println("  mov %%rax, (%%rdi)"); return;
  }
}

static void cmp_zero(Type *ty) {
  (void)ty;
  println("  cmp $0, %%rax");
}

// Converts the value in %rax to type `to`. Because values are kept sign-extended, narrowing is
// a sign-extension of the low bits and widening needs no instruction at all.
static void cast(Type *from, Type *to) {
  if (to->kind == TY_VOID)
    return;
  // Widening (char -> int, int -> long) or same-size casts need no code: the value in %rax
  // is already sign-extended. Skipping them removes most redundant instructions.
  if (is_integer(from) && is_integer(to) && from->size <= to->size)
    return;
  switch (to->kind) {
  case TY_CHAR: println("  movsbq %%al, %%rax"); return;
  case TY_SHORT: println("  movswq %%ax, %%rax"); return;
  case TY_INT: println("  movslq %%eax, %%rax"); return;
  default: return; // long, pointers: already 64-bit
  }
}

static void store_param(int reg, int offset, int size) {
  switch (size) {
  case 1: println("  mov %s, %d(%%rbp)", argreg8[reg], offset); return;
  case 2: println("  mov %s, %d(%%rbp)", argreg16[reg], offset); return;
  case 4: println("  mov %s, %d(%%rbp)", argreg32[reg], offset); return;
  default: println("  mov %s, %d(%%rbp)", argreg64[reg], offset); return;
  }
}

static void gen_expr(Node *node) {
  println("  .loc 1 %d", node->tok->line_no); // line info, so gdb can map assembly to source

  switch (node->kind) {
  case ND_NUM:
    if (node->val == (int)node->val)
      println("  mov $%ld, %%rax", node->val);
    else
      println("  movabs $%ld, %%rax", node->val);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    if (node->ty->size == 8) {
      println("  neg %%rax");
    } else {
      println("  neg %%eax");
      println("  movslq %%eax, %%rax");
    }
    return;
  case ND_VAR:
  case ND_MEMBER:
    gen_addr(node);
    load(node->ty);
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    load(node->ty);
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_ASSIGN:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    store(node->ty);
    return;
  case ND_COMMA:
    gen_expr(node->lhs);
    gen_expr(node->rhs);
    return;
  case ND_CAST:
    gen_expr(node->lhs);
    cast(node->lhs->ty, node->ty);
    return;
  case ND_MEMZERO:
    // Fill the variable with zero bytes: rep stosb writes %al into (%rdi), %rcx times.
    println("  mov $%d, %%rcx", node->var->ty->size);
    println("  lea %d(%%rbp), %%rdi", node->var->offset);
    println("  mov $0, %%al");
    println("  rep stosb");
    return;
  case ND_COND: {
    int c = count();
    gen_expr(node->cond);
    cmp_zero(node->cond->ty);
    println("  je .L.else.%d", c);
    gen_expr(node->then);
    println("  jmp .L.end.%d", c);
    println(".L.else.%d:", c);
    gen_expr(node->els);
    println(".L.end.%d:", c);
    return;
  }
  case ND_NOT:
    gen_expr(node->lhs);
    cmp_zero(node->lhs->ty);
    println("  sete %%al");
    println("  movzx %%al, %%rax");
    return;
  case ND_BITNOT:
    gen_expr(node->lhs);
    println("  not %%rax");
    return;
  case ND_LOGAND: { // short-circuit: if the left side is 0, the right side is never evaluated
    int c = count();
    gen_expr(node->lhs);
    cmp_zero(node->lhs->ty);
    println("  je .L.false.%d", c);
    gen_expr(node->rhs);
    cmp_zero(node->rhs->ty);
    println("  je .L.false.%d", c);
    println("  mov $1, %%rax");
    println("  jmp .L.end.%d", c);
    println(".L.false.%d:", c);
    println("  mov $0, %%rax");
    println(".L.end.%d:", c);
    return;
  }
  case ND_LOGOR: {
    int c = count();
    gen_expr(node->lhs);
    cmp_zero(node->lhs->ty);
    println("  jne .L.true.%d", c);
    gen_expr(node->rhs);
    cmp_zero(node->rhs->ty);
    println("  jne .L.true.%d", c);
    println("  mov $0, %%rax");
    println("  jmp .L.end.%d", c);
    println(".L.true.%d:", c);
    println("  mov $1, %%rax");
    println(".L.end.%d:", c);
    return;
  }
  case ND_FUNCALL: {
    // System V ABI: the first six integer arguments go in rdi, rsi, rdx, rcx, r8, r9.
    // Any more are passed on the stack, 7th argument at the lowest address.
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next)
      nargs++;
    Node **args = calloc(nargs ? nargs : 1, sizeof(Node *));
    int k = 0;
    for (Node *arg = node->args; arg; arg = arg->next)
      args[k++] = arg;
    int stack_args = nargs > 6 ? nargs - 6 : 0;

    // %rsp must be 16-byte aligned at the call instruction. Pad first if the stack arguments
    // would leave it misaligned.
    bool pad = (depth + stack_args) % 2 != 0;
    if (pad) {
      println("  sub $8, %%rsp");
      depth++;
    }
    // Push right to left, so after popping the first six into registers the 7th is on top.
    for (int i = nargs - 1; i >= 0; i--) {
      gen_expr(args[i]);
      push();
    }
    for (int i = 0; i < nargs && i < 6; i++)
      pop(argreg64[i]);
    free(args);

    println("  mov $0, %%eax"); // number of vector registers used by a variadic call: none
    println("  call %s@PLT", node->funcname);
    int cleanup = stack_args + (pad ? 1 : 0);
    if (cleanup) {
      println("  add $%d, %%rsp", cleanup * 8);
      depth -= cleanup;
    }
    // The callee only guarantees the low bits of the return value; sign-extend them.
    switch (node->ty->kind) {
    case TY_CHAR: println("  movsbq %%al, %%rax"); break;
    case TY_SHORT: println("  movswq %%ax, %%rax"); break;
    case TY_INT: println("  movslq %%eax, %%rax"); break;
    default: break;
    }
    return;
  }
  default:
    break;
  }

  // Binary operators: right side to the stack, left side to %rax, right side into %rdi.
  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("%rdi");

  bool wide = node->lhs->ty->kind == TY_LONG || node->lhs->ty->base;
  char *ax = wide ? "%rax" : "%eax";
  char *di = wide ? "%rdi" : "%edi";
  char *dx = wide ? "%rdx" : "%edx";

  switch (node->kind) {
  case ND_ADD: println("  add %s, %s", di, ax); break;
  case ND_SUB: println("  sub %s, %s", di, ax); break;
  case ND_MUL: println("  imul %s, %s", di, ax); break;
  case ND_DIV:
  case ND_MOD:
    // idiv divides rdx:rax by the operand; cqo/cdq sign-extend rax into rdx first.
    println(wide ? "  cqo" : "  cdq");
    println("  idiv %s", di);
    if (node->kind == ND_MOD)
      println("  mov %s, %s", dx, ax);
    break;
  case ND_BITAND: println("  and %s, %s", di, ax); break;
  case ND_BITOR: println("  or %s, %s", di, ax); break;
  case ND_BITXOR: println("  xor %s, %s", di, ax); break;
  case ND_SHL:
    println("  mov %%rdi, %%rcx");
    println("  shl %%cl, %s", ax);
    break;
  case ND_SHR:
    println("  mov %%rdi, %%rcx");
    println("  sar %%cl, %s", ax); // arithmetic shift: jcc's integers are signed
    break;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    println("  cmp %%rdi, %%rax"); // both sides are sign-extended, so a 64-bit compare is exact
    if (node->kind == ND_EQ)
      println("  sete %%al");
    else if (node->kind == ND_NE)
      println("  setne %%al");
    else if (node->kind == ND_LT)
      println("  setl %%al");
    else
      println("  setle %%al");
    println("  movzb %%al, %%rax");
    return;
  default:
    error_tok(node->tok, "invalid expression");
  }
  if (!wide)
    println("  movslq %%eax, %%rax"); // keep the int result sign-extended
}

static void gen_stmt(Node *node) {
  println("  .loc 1 %d", node->tok->line_no);

  switch (node->kind) {
  case ND_IF: {
    int c = count();
    gen_expr(node->cond);
    cmp_zero(node->cond->ty);
    println("  je .L.else.%d", c);
    gen_stmt(node->then);
    println("  jmp .L.end.%d", c);
    println(".L.else.%d:", c);
    if (node->els)
      gen_stmt(node->els);
    println(".L.end.%d:", c);
    return;
  }
  case ND_FOR: {
    // init; begin: if (!cond) goto brk; body; cont: inc; goto begin; brk:
    int c = count();
    if (node->init)
      gen_stmt(node->init);
    println(".L.begin.%d:", c);
    if (node->cond) {
      gen_expr(node->cond);
      cmp_zero(node->cond->ty);
      println("  je %s", node->brk_label);
    }
    gen_stmt(node->then);
    println("%s:", node->cont_label);
    if (node->inc)
      gen_expr(node->inc);
    println("  jmp .L.begin.%d", c);
    println("%s:", node->brk_label);
    return;
  }
  case ND_DO: {
    int c = count();
    println(".L.begin.%d:", c);
    gen_stmt(node->then);
    println("%s:", node->cont_label);
    gen_expr(node->cond);
    cmp_zero(node->cond->ty);
    println("  jne .L.begin.%d", c);
    println("%s:", node->brk_label);
    return;
  }
  case ND_SWITCH:
    // Compare against each case value in turn and jump to the matching label.
    gen_expr(node->cond);
    for (Node *n = node->case_next; n; n = n->case_next) {
      if (n->case_val == (int)n->case_val) {
        println("  cmp $%ld, %%rax", n->case_val);
      } else {
        println("  movabs $%ld, %%rdi", n->case_val);
        println("  cmp %%rdi, %%rax");
      }
      println("  je %s", n->label);
    }
    if (node->default_case)
      println("  jmp %s", node->default_case->label);
    println("  jmp %s", node->brk_label);
    gen_stmt(node->then);
    println("%s:", node->brk_label);
    return;
  case ND_CASE:
    println("%s:", node->label);
    gen_stmt(node->lhs);
    return;
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_BREAK:
  case ND_CONTINUE:
    println("  jmp %s", node->label);
    return;
  case ND_RETURN:
    if (node->lhs)
      gen_expr(node->lhs);
    println("  jmp .L.return.%s", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  default:
    error_tok(node->tok, "invalid statement");
  }
}

// Gives every local variable a slot in the function's stack frame, below %rbp.
static void assign_lvar_offsets(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function)
      continue;
    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      offset = align_to(offset, var->ty->align);
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

static void emit_data(Obj *prog) {
  for (Obj *var = prog; var; var = var->next) {
    if (var->is_function || !var->is_definition)
      continue;
    bool is_anon = strncmp(var->name, ".L", 2) == 0;
    if (!is_anon)
      println("  .globl %s", var->name);
    int align = var->ty->align < 8 && var->ty->kind == TY_ARRAY && var->ty->size >= 16 ? 16 : var->ty->align;

    if (var->init_data) {
      println("  .data");
      println("  .align %d", align);
      println("%s:", var->name);
      Relocation *rel = var->rel;
      int pos = 0;
      while (pos < var->ty->size) {
        if (rel && rel->offset == pos) {
          println("  .quad %s%+ld", rel->label, rel->addend);
          rel = rel->next;
          pos += 8;
        } else {
          println("  .byte %d", (unsigned char)var->init_data[pos++]);
        }
      }
      continue;
    }
    println("  .bss");
    println("  .align %d", align);
    println("%s:", var->name);
    println("  .zero %d", var->ty->size);
  }
}

static void emit_text(Obj *prog) {
  for (Obj *fn = prog; fn; fn = fn->next) {
    if (!fn->is_function || !fn->is_definition)
      continue;
    println("  .globl %s", fn->name);
    println("  .text");
    println("  .type %s, @function", fn->name);
    println("%s:", fn->name);
    current_fn = fn;

    // Prologue: save the caller's frame pointer and reserve space for locals.
    println("  push %%rbp");
    println("  mov %%rsp, %%rbp");
    println("  sub $%d, %%rsp", fn->stack_size);

    // Copy register arguments into the parameters' stack slots.
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next) {
      if (i >= 6)
        error_tok(fn->tok, "jcc supports at most 6 parameters per function");
      store_param(i++, var->offset, var->ty->size);
    }

    gen_stmt(fn->body);
    if (depth != 0)
      error("internal error: stack depth %d after %s", depth, fn->name);

    // Reaching the end of main() returns 0, as the C standard requires.
    if (strcmp(fn->name, "main") == 0)
      println("  mov $0, %%rax");

    // Epilogue
    println(".L.return.%s:", fn->name);
    println("  mov %%rbp, %%rsp");
    println("  pop %%rbp");
    println("  ret");
  }
}

void codegen(Obj *prog, FILE *out, char *source_path) {
  output_file = out;
  println("  .file 1 \"%s\"", source_path); // lets .loc lines refer to the C source
  assign_lvar_offsets(prog);
  emit_data(prog);
  emit_text(prog);
  // Mark the stack as non-executable (avoids a linker warning and is good security practice).
  println("  .section .note.GNU-stack,\"\",@progbits");
}
