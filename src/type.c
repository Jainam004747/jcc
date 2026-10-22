// The type system: what types exist, their sizes, and the type of every expression.
//
// add_type() walks the AST bottom-up and fills in node->ty. This is where C's rules live:
// "int + long is long", "pointer + int is a pointer", "an array used as a value becomes a
// pointer to its first element", and "*p on an int gives an error".

#include "jcc.h"

Type *ty_void = &(Type){.kind = TY_VOID, .size = 1, .align = 1};
Type *ty_char = &(Type){.kind = TY_CHAR, .size = 1, .align = 1};
Type *ty_short = &(Type){.kind = TY_SHORT, .size = 2, .align = 2};
Type *ty_int = &(Type){.kind = TY_INT, .size = 4, .align = 4};
Type *ty_long = &(Type){.kind = TY_LONG, .size = 8, .align = 8};

static Type *new_type(TypeKind kind, int size, int align) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = kind;
  ty->size = size;
  ty->align = align;
  return ty;
}

bool is_integer(Type *ty) {
  TypeKind k = ty->kind;
  return k == TY_CHAR || k == TY_SHORT || k == TY_INT || k == TY_LONG;
}

// Pointers and arrays both have a base type and can be indexed and dereferenced.
bool is_pointer_like(Type *ty) {
  return ty->base != NULL;
}

int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

Type *copy_type(Type *ty) {
  Type *ret = calloc(1, sizeof(Type));
  *ret = *ty;
  return ret;
}

Type *pointer_to(Type *base) {
  Type *ty = new_type(TY_PTR, 8, 8);
  ty->base = base;
  return ty;
}

Type *func_type(Type *return_ty) {
  Type *ty = new_type(TY_FUNC, 1, 1);
  ty->return_ty = return_ty;
  return ty;
}

Type *array_of(Type *base, int len) {
  Type *ty = new_type(TY_ARRAY, base->size * len, base->align);
  ty->base = base;
  ty->array_len = len;
  return ty;
}

Type *struct_type(void) {
  return new_type(TY_STRUCT, 0, 1);
}

// The "usual arithmetic conversions", simplified for jcc's types:
// anything smaller than int becomes int, and if either side is long the result is long.
static Type *common_type(Type *a, Type *b) {
  if (a->base)
    return pointer_to(a->base);
  if (a->size == 8 || b->size == 8)
    return ty_long;
  return ty_int;
}

// Converts both operands of a binary operator to their common type, so codegen can
// use one instruction width for the whole operation.
static void usual_arith_conv(Node **lhs, Node **rhs) {
  Type *ty = common_type((*lhs)->ty, (*rhs)->ty);
  *lhs = new_cast(*lhs, ty);
  *rhs = new_cast(*rhs, ty);
}

void add_type(Node *node) {
  if (!node || node->ty)
    return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->inc);
  for (Node *n = node->body; n; n = n->next)
    add_type(n);
  for (Node *n = node->args; n; n = n->next)
    add_type(n);

  switch (node->kind) {
  case ND_NUM:
    node->ty = (node->val == (int)node->val) ? ty_int : ty_long;
    return;
  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
  case ND_MOD:
  case ND_BITAND:
  case ND_BITOR:
  case ND_BITXOR:
    usual_arith_conv(&node->lhs, &node->rhs);
    node->ty = node->lhs->ty;
    return;
  case ND_NEG: {
    Type *ty = common_type(ty_int, node->lhs->ty);
    node->lhs = new_cast(node->lhs, ty);
    node->ty = ty;
    return;
  }
  case ND_ASSIGN:
    if (node->lhs->ty->kind == TY_ARRAY)
      error_tok(node->lhs->tok, "cannot assign to an array");
    if (node->lhs->ty->kind != TY_STRUCT)
      node->rhs = new_cast(node->rhs, node->lhs->ty);
    node->ty = node->lhs->ty;
    return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    usual_arith_conv(&node->lhs, &node->rhs);
    node->ty = ty_int;
    return;
  case ND_FUNCALL:
    node->ty = node->func_ty->return_ty;
    return;
  case ND_NOT:
  case ND_LOGOR:
  case ND_LOGAND:
    node->ty = ty_int;
    return;
  case ND_BITNOT:
  case ND_SHL:
  case ND_SHR:
    node->ty = node->lhs->ty->size == 8 ? ty_long : ty_int;
    node->lhs = new_cast(node->lhs, node->ty);
    return;
  case ND_VAR:
    node->ty = node->var->ty;
    return;
  case ND_COND:
    if (node->then->ty->kind == TY_VOID || node->els->ty->kind == TY_VOID) {
      node->ty = ty_void;
    } else {
      usual_arith_conv(&node->then, &node->els);
      node->ty = node->then->ty;
    }
    return;
  case ND_COMMA:
    node->ty = node->rhs->ty;
    return;
  case ND_MEMBER:
    node->ty = node->member->ty;
    return;
  case ND_ADDR: {
    Type *ty = node->lhs->ty;
    // &array has the array's element pointer type here (simplification: &a == a)
    node->ty = ty->kind == TY_ARRAY ? pointer_to(ty->base) : pointer_to(ty);
    return;
  }
  case ND_DEREF:
    if (!node->lhs->ty->base)
      error_tok(node->tok, "cannot dereference something that is not a pointer");
    if (node->lhs->ty->base->kind == TY_VOID)
      error_tok(node->tok, "cannot dereference a void pointer");
    node->ty = node->lhs->ty->base;
    return;
  default:
    return;
  }
}
