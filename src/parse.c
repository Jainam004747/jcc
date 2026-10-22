// Parser: tokens -> abstract syntax tree, using recursive descent.
//
// Each grammar rule below is one C function. Operator precedence comes from the call order:
// add() calls mul(), so in "1 + 2 * 3" the multiplication is parsed first and ends up deeper
// in the tree. The grammar, from lowest to highest precedence:
//
//   program     = (typedef | function-definition | global-variable)*
//   stmt        = "return" expr? ";" | "if" ... | "for" ... | "while" ... | "do" ... | "switch" ...
//               | "case" const-expr ":" stmt | "default" ":" stmt | "break" ";" | "continue" ";"
//               | "{" compound-stmt | expr? ";"
//   expr        = assign ("," expr)?
//   assign      = conditional (assign-op assign)?          assign-op: = += -= *= /= %= &= |= ^= <<= >>=
//   conditional = logor ("?" expr ":" conditional)?
//   logor       = logand ("||" logand)*
//   logand      = bitor ("&&" bitor)*
//   bitor       = bitxor ("|" bitxor)*
//   bitxor      = bitand ("^" bitand)*
//   bitand      = equality ("&" equality)*
//   equality    = relational ("==" relational | "!=" relational)*
//   relational  = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
//   shift       = add ("<<" add | ">>" add)*
//   add         = mul ("+" mul | "-" mul)*
//   mul         = cast ("*" cast | "/" cast | "%" cast)*
//   cast        = "(" type-name ")" cast | unary
//   unary       = ("+" | "-" | "*" | "&" | "!" | "~") cast | ("++" | "--") unary
//               | "sizeof" "(" type-name ")" | "sizeof" unary | postfix
//   postfix     = primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
//   primary     = "(" expr ")" | ident func-args? | str | num

#include "jcc.h"

// ---- scopes ---------------------------------------------------------------
// Every block { } opens a scope. A name is looked up from the innermost scope outwards,
// which is how a local variable can shadow a global with the same name.

typedef struct VarScope VarScope;
struct VarScope {
  VarScope *next;
  char *name;
  Obj *var;        // a variable or function...
  Type *type_def;  // ...or a typedef name
};

typedef struct TagScope TagScope;
struct TagScope {
  TagScope *next;
  char *name;
  Type *ty;
};

typedef struct Scope Scope;
struct Scope {
  Scope *next;
  VarScope *vars;
  TagScope *tags; // struct tags live in their own namespace: "struct point" vs "point"
};

typedef struct {
  bool is_typedef;
  bool is_static;
} VarAttr;

// An initializer such as {1, 2, {3, 4}} parsed into a tree that mirrors the variable's type.
typedef struct Initializer Initializer;
struct Initializer {
  Type *ty;
  Token *tok;
  Node *expr;            // scalar: the value
  Initializer **children; // array or struct: one per element / member
  int nchildren;
  bool is_flexible;       // int a[] = {...}: length comes from the initializer
};

static Obj *locals;   // locals of the function being parsed
static Obj *globals;  // all globals and functions
static Scope *scope = &(Scope){0};
static Obj *current_fn;
static char *brk_label;
static char *cont_label;
static Node *current_switch;

static Type *declspec(Token **rest, Token *tok, VarAttr *attr);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Type *abstract_declarator(Token **rest, Token *tok, Type *ty);
static Type *typename_(Token **rest, Token *tok);
static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr);
static bool is_typename(Token *tok);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *conditional(Token **rest, Token *tok);
static Node *logor(Token **rest, Token *tok);
static Node *logand(Token **rest, Token *tok);
static Node *bitor(Token **rest, Token *tok);
static Node *bitxor(Token **rest, Token *tok);
static Node *bitand(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *shift(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *new_add(Node *lhs, Node *rhs, Token *tok);
static Node *new_sub(Node *lhs, Node *rhs, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *cast(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);
static Type *struct_decl(Token **rest, Token *tok);

static void enter_scope(void) {
  Scope *sc = calloc(1, sizeof(Scope));
  sc->next = scope;
  scope = sc;
}

static void leave_scope(void) {
  scope = scope->next;
}

static char *get_ident(Token *tok) {
  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected an identifier");
  return strndup(tok->loc, tok->len);
}

static VarScope *find_var(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next)
    for (VarScope *vs = sc->vars; vs; vs = vs->next)
      if (equal(tok, vs->name))
        return vs;
  return NULL;
}

static Type *find_tag(Token *tok) {
  for (Scope *sc = scope; sc; sc = sc->next)
    for (TagScope *ts = sc->tags; ts; ts = ts->next)
      if (equal(tok, ts->name))
        return ts->ty;
  return NULL;
}

static Type *find_typedef(Token *tok) {
  if (tok->kind == TK_IDENT) {
    VarScope *vs = find_var(tok);
    if (vs)
      return vs->type_def;
  }
  return NULL;
}

static VarScope *push_scope(char *name) {
  VarScope *vs = calloc(1, sizeof(VarScope));
  vs->name = name;
  vs->next = scope->vars;
  scope->vars = vs;
  return vs;
}

static void push_tag_scope(Token *tok, Type *ty) {
  TagScope *ts = calloc(1, sizeof(TagScope));
  ts->name = strndup(tok->loc, tok->len);
  ts->ty = ty;
  ts->next = scope->tags;
  scope->tags = ts;
}

// ---- node constructors -----------------------------------------------------

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

static Node *new_num(int64_t val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node *new_long(int64_t val, Token *tok) {
  Node *node = new_num(val, tok);
  node->ty = ty_long;
  return node;
}

static Node *new_var_node(Obj *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

Node *new_cast(Node *expr, Type *ty) {
  add_type(expr);
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_CAST;
  node->tok = expr->tok;
  node->lhs = expr;
  node->ty = copy_type(ty);
  return node;
}

static Obj *new_var(char *name, Type *ty) {
  Obj *var = calloc(1, sizeof(Obj));
  var->name = name;
  var->ty = ty;
  push_scope(name)->var = var;
  return var;
}

static Obj *new_lvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->is_local = true;
  var->next = locals;
  locals = var;
  return var;
}

static Obj *new_gvar(char *name, Type *ty) {
  Obj *var = new_var(name, ty);
  var->next = globals;
  globals = var;
  return var;
}

static char *new_unique_name(void) {
  static int id = 0;
  return format(".L..%d", id++);
}

static Obj *new_anon_gvar(Type *ty) {
  return new_gvar(new_unique_name(), ty);
}

static Obj *new_string_literal(char *p, Type *ty) {
  Obj *var = new_anon_gvar(ty);
  var->init_data = p;
  var->is_definition = true;
  return var;
}

// ---- declarations ------------------------------------------------------------

static void unsupported(Token *tok) {
  error_tok(tok, "'%.*s' is not supported by jcc (see README: supported subset)", tok->len, tok->loc);
}

// declspec = ("void" | "char" | "short" | "int" | "long" | struct-decl | typedef-name
//             | "typedef" | "static" | "extern" | "const" | "signed")+
//
// Counts each base-type keyword so that "long int", "long long" and "short int" work.
static Type *declspec(Token **rest, Token *tok, VarAttr *attr) {
  enum { VOID = 1 << 0, CHAR = 1 << 2, SHORT = 1 << 4, INT = 1 << 6, LONG = 1 << 8, OTHER = 1 << 12 };
  Type *ty = ty_int;
  int counter = 0;

  while (is_typename(tok)) {
    if (equal(tok, "typedef") || equal(tok, "static")) {
      if (!attr)
        error_tok(tok, "storage class specifier is not allowed here");
      if (equal(tok, "typedef"))
        attr->is_typedef = true;
      else
        attr->is_static = true;
      tok = tok->next;
      continue;
    }
    if (equal(tok, "const") || equal(tok, "extern") || equal(tok, "signed")) {
      tok = tok->next; // accepted and ignored
      continue;
    }
    if (equal(tok, "unsigned") || equal(tok, "float") || equal(tok, "double") ||
        equal(tok, "union") || equal(tok, "enum"))
      unsupported(tok);

    Type *ty2 = find_typedef(tok);
    if (equal(tok, "struct") || ty2) {
      if (counter)
        break;
      if (equal(tok, "struct")) {
        ty = struct_decl(&tok, tok->next);
      } else {
        ty = ty2;
        tok = tok->next;
      }
      counter += OTHER;
      continue;
    }

    if (equal(tok, "void"))
      counter += VOID;
    else if (equal(tok, "char"))
      counter += CHAR;
    else if (equal(tok, "short"))
      counter += SHORT;
    else if (equal(tok, "int"))
      counter += INT;
    else if (equal(tok, "long"))
      counter += LONG;

    switch (counter) {
    case VOID: ty = ty_void; break;
    case CHAR: ty = ty_char; break;
    case SHORT:
    case SHORT + INT: ty = ty_short; break;
    case INT: ty = ty_int; break;
    case LONG:
    case LONG + INT:
    case LONG + LONG:
    case LONG + LONG + INT: ty = ty_long; break;
    default: error_tok(tok, "invalid combination of type keywords");
    }
    tok = tok->next;
  }
  *rest = tok;
  return ty;
}

// func-params = ("void" | param ("," param)* ("," "...")?)? ")"
// param       = declspec declarator
static Type *func_params(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "void") && equal(tok->next, ")")) {
    *rest = tok->next->next;
    return func_type(ty);
  }
  Type head = {0};
  Type *cur = &head;
  bool is_variadic = false;
  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");
    if (equal(tok, "...")) {
      is_variadic = true;
      tok = tok->next;
      skip(tok, ")");
      break;
    }
    Type *ty2 = declspec(&tok, tok, NULL);
    ty2 = declarator(&tok, tok, ty2);
    Token *name = ty2->name;
    if (ty2->kind == TY_ARRAY) {
      // An array parameter is really a pointer: int f(int a[10]) == int f(int *a)
      ty2 = pointer_to(ty2->base);
      ty2->name = name;
    }
    cur = cur->next = copy_type(ty2);
  }
  ty = func_type(ty);
  ty->params = head.next;
  ty->is_variadic = is_variadic;
  *rest = tok->next;
  return ty;
}

// array-dimensions = const-expr? "]" type-suffix
static Type *array_dimensions(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "]")) {
    ty = abstract_declarator(rest, tok->next, ty); // reuse type-suffix parsing
    return array_of(ty, -1);                         // length decided by the initializer
  }
  Token *start = tok;
  int64_t len = const_expr(&tok, tok);
  if (len <= 0)
    error_tok(start, "array size must be positive");
  tok = skip(tok, "]");
  if (equal(tok, "[")) {
    ty = array_dimensions(&tok, tok->next, ty);
  } else if (equal(tok, "(")) {
    error_tok(tok, "arrays of functions are not allowed");
  }
  *rest = tok;
  return array_of(ty, (int)len);
}

// type-suffix = "(" func-params | "[" array-dimensions | ε
static Type *type_suffix(Token **rest, Token *tok, Type *ty) {
  if (equal(tok, "("))
    return func_params(rest, tok->next, ty);
  if (equal(tok, "["))
    return array_dimensions(rest, tok->next, ty);
  *rest = tok;
  return ty;
}

// declarator = "*"* ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty) {
  while (consume(&tok, tok, "*")) {
    ty = pointer_to(ty);
    while (consume(&tok, tok, "const"))
      ;
  }
  if (equal(tok, "("))
    error_tok(tok, "function pointers and parenthesised declarators are not supported by jcc");
  if (tok->kind != TK_IDENT)
    error_tok(tok, "expected a variable name");
  Token *name = tok;
  ty = type_suffix(rest, tok->next, ty);
  ty->name = name;
  return ty;
}

// abstract-declarator = "*"* type-suffix   (a declarator without a name, as in casts)
static Type *abstract_declarator(Token **rest, Token *tok, Type *ty) {
  while (consume(&tok, tok, "*")) {
    ty = pointer_to(ty);
    while (consume(&tok, tok, "const"))
      ;
  }
  return type_suffix(rest, tok, ty);
}

static Type *typename_(Token **rest, Token *tok) {
  Type *ty = declspec(&tok, tok, NULL);
  return abstract_declarator(rest, tok, ty);
}

static bool is_typename(Token *tok) {
  static char *kw[] = {
    "void", "char", "short", "int", "long", "struct", "typedef", "static", "extern", "const",
    "signed", "unsigned", "float", "double", "union", "enum",
  };
  for (size_t i = 0; i < sizeof(kw) / sizeof(*kw); i++)
    if (equal(tok, kw[i]))
      return true;
  return find_typedef(tok) != NULL;
}

// struct-decl = ident? ("{" struct-members "}")?
static Type *struct_decl(Token **rest, Token *tok) {
  Token *tag = NULL;
  if (tok->kind == TK_IDENT) {
    tag = tok;
    tok = tok->next;
  }

  if (tag && !equal(tok, "{")) {
    *rest = tok;
    Type *ty = find_tag(tag);
    if (ty)
      return ty;
    // Forward reference: "struct node *next;" before struct node is complete.
    ty = struct_type();
    ty->size = -1;
    push_tag_scope(tag, ty);
    return ty;
  }

  tok = skip(tok, "{");

  // Parse members.
  Member head = {0};
  Member *cur = &head;
  while (!equal(tok, "}")) {
    Type *basety = declspec(&tok, tok, NULL);
    bool first = true;
    while (!consume(&tok, tok, ";")) {
      if (!first)
        tok = skip(tok, ",");
      first = false;
      Member *mem = calloc(1, sizeof(Member));
      mem->ty = declarator(&tok, tok, basety);
      mem->name = mem->ty->name;
      if (mem->ty->size < 0)
        error_tok(mem->name, "member has an incomplete type");
      cur = cur->next = mem;
    }
  }
  *rest = tok->next;

  // If this tag was forward-declared in the current scope, complete that same Type object, so
  // pointers created earlier ("struct node *next") see the members too.
  Type *ty = NULL;
  if (tag) {
    for (TagScope *ts = scope->tags; ts; ts = ts->next) {
      if (equal(tag, ts->name) && ts->ty->kind == TY_STRUCT) {
        ty = ts->ty;
        break;
      }
    }
  }
  if (!ty) {
    ty = struct_type();
    if (tag)
      push_tag_scope(tag, ty);
  } else if (ty->is_complete) {
    error_tok(tag, "redefinition of struct %.*s", tag->len, tag->loc);
  }

  // Lay out members: each one starts at the next offset that satisfies its alignment.
  //   struct { char c; int i; }  ->  c at 0, 3 bytes padding, i at 4, size 8
  int offset = 0;
  ty->align = 1;
  for (Member *mem = head.next; mem; mem = mem->next) {
    offset = align_to(offset, mem->ty->align);
    mem->offset = offset;
    offset += mem->ty->size;
    if (ty->align < mem->ty->align)
      ty->align = mem->ty->align;
  }
  ty->members = head.next;
  ty->size = align_to(offset, ty->align);
  ty->is_complete = true;
  return ty;
}

static Member *get_struct_member(Type *ty, Token *tok) {
  for (Member *mem = ty->members; mem; mem = mem->next)
    if (mem->name->len == tok->len && !memcmp(mem->name->loc, tok->loc, tok->len))
      return mem;
  error_tok(tok, "no member named '%.*s'", tok->len, tok->loc);
}

static Node *struct_ref(Node *node, Token *tok) {
  add_type(node);
  if (node->ty->kind != TY_STRUCT)
    error_tok(node->tok, "member access on something that is not a struct");
  if (!node->ty->is_complete)
    error_tok(node->tok, "member access on an incomplete struct");
  Node *ref = new_unary(ND_MEMBER, node, tok);
  ref->member = get_struct_member(node->ty, tok);
  return ref;
}

// ---- initializers ------------------------------------------------------------

static Initializer *new_initializer(Type *ty, bool is_flexible) {
  Initializer *init = calloc(1, sizeof(Initializer));
  init->ty = ty;
  if (ty->kind == TY_ARRAY) {
    if (is_flexible && ty->array_len < 0) {
      init->is_flexible = true;
      return init;
    }
    init->nchildren = ty->array_len;
    init->children = calloc(ty->array_len, sizeof(Initializer *));
    for (int i = 0; i < ty->array_len; i++)
      init->children[i] = new_initializer(ty->base, false);
  } else if (ty->kind == TY_STRUCT) {
    int n = 0;
    for (Member *m = ty->members; m; m = m->next)
      n++;
    init->nchildren = n;
    init->children = calloc(n, sizeof(Initializer *));
    int i = 0;
    for (Member *m = ty->members; m; m = m->next)
      init->children[i++] = new_initializer(m->ty, false);
  }
  return init;
}

static void initializer2(Token **rest, Token *tok, Initializer *init);

// Grows a flexible array initializer to hold `n` elements.
static void ensure_children(Initializer *init, int n) {
  if (n <= init->nchildren)
    return;
  init->children = realloc(init->children, n * sizeof(Initializer *));
  for (int i = init->nchildren; i < n; i++)
    init->children[i] = new_initializer(init->ty->base, false);
  init->nchildren = n;
}

// string-initializer = string-literal   (for char arrays: char s[] = "hello")
static void string_initializer(Token **rest, Token *tok, Initializer *init) {
  int len = tok->ty->array_len;
  if (init->is_flexible) {
    ensure_children(init, len);
    init->ty = array_of(init->ty->base, len);
  }
  int n = len < init->nchildren ? len : init->nchildren;
  for (int i = 0; i < n; i++)
    init->children[i]->expr = new_num(tok->str[i], tok);
  *rest = tok->next;
}

// array-initializer = "{" initializer ("," initializer)* ","? "}"
static void array_initializer(Token **rest, Token *tok, Initializer *init) {
  tok = skip(tok, "{");
  int i = 0;
  while (!consume(rest, tok, "}")) {
    if (i > 0)
      tok = skip(tok, ",");
    if (consume(rest, tok, "}"))
      break; // trailing comma
    if (init->is_flexible)
      ensure_children(init, i + 1);
    if (i < init->nchildren) {
      initializer2(&tok, tok, init->children[i]);
    } else {
      warn_tok(tok, "excess elements in array initializer are ignored");
      Initializer *dummy = new_initializer(init->ty->base, false);
      initializer2(&tok, tok, dummy);
    }
    i++;
  }
  if (init->is_flexible)
    init->ty = array_of(init->ty->base, init->nchildren);
}

// struct-initializer = "{" initializer ("," initializer)* ","? "}"
static void struct_initializer(Token **rest, Token *tok, Initializer *init) {
  tok = skip(tok, "{");
  int i = 0;
  while (!consume(rest, tok, "}")) {
    if (i > 0)
      tok = skip(tok, ",");
    if (consume(rest, tok, "}"))
      break;
    if (i >= init->nchildren)
      error_tok(tok, "too many initializers for struct");
    initializer2(&tok, tok, init->children[i]);
    i++;
  }
}

// initializer = string-initializer | array-initializer | struct-initializer | assign
static void initializer2(Token **rest, Token *tok, Initializer *init) {
  if (init->ty->kind == TY_ARRAY && tok->kind == TK_STR && init->ty->base->kind == TY_CHAR) {
    string_initializer(rest, tok, init);
    return;
  }
  if (init->ty->kind == TY_ARRAY) {
    if (!equal(tok, "{"))
      error_tok(tok, "array initializer must be in braces { }");
    array_initializer(rest, tok, init);
    return;
  }
  if (init->ty->kind == TY_STRUCT && equal(tok, "{")) {
    struct_initializer(rest, tok, init);
    return;
  }
  if (equal(tok, "{")) { // int x = {5};
    initializer2(&tok, tok->next, init);
    *rest = skip(tok, "}");
    return;
  }
  init->expr = assign(rest, tok);
}

static Initializer *initializer(Token **rest, Token *tok, Type *ty, Type **new_ty) {
  Initializer *init = new_initializer(ty, true);
  initializer2(rest, tok, init);
  *new_ty = init->ty;
  return init;
}

// Turns a local initializer into assignments: int a[2] = {1, 2}; -> a[0] = 1, a[1] = 2
typedef struct InitDesg InitDesg;
struct InitDesg {
  InitDesg *next;
  int idx;
  Member *member;
  Obj *var;
};

static Node *init_desg_expr(InitDesg *desg, Token *tok) {
  if (desg->var)
    return new_var_node(desg->var, tok);
  if (desg->member) {
    Node *node = new_unary(ND_MEMBER, init_desg_expr(desg->next, tok), tok);
    node->member = desg->member;
    return node;
  }
  Node *lhs = init_desg_expr(desg->next, tok);
  Node *rhs = new_num(desg->idx, tok);
  return new_unary(ND_DEREF, new_add(lhs, rhs, tok), tok);
}

static Node *create_lvar_init(Initializer *init, Type *ty, InitDesg *desg, Token *tok) {
  if (ty->kind == TY_ARRAY) {
    Node *node = new_node(ND_NUM, tok); // a no-op starting point for the comma chain
    for (int i = 0; i < ty->array_len; i++) {
      InitDesg desg2 = {.next = desg, .idx = i};
      Node *rhs = create_lvar_init(init->children[i], ty->base, &desg2, tok);
      node = new_binary(ND_COMMA, node, rhs, tok);
    }
    return node;
  }
  if (ty->kind == TY_STRUCT && !init->expr) {
    Node *node = new_node(ND_NUM, tok);
    int i = 0;
    for (Member *mem = ty->members; mem; mem = mem->next, i++) {
      InitDesg desg2 = {.next = desg, .member = mem};
      Node *rhs = create_lvar_init(init->children[i], mem->ty, &desg2, tok);
      node = new_binary(ND_COMMA, node, rhs, tok);
    }
    return node;
  }
  if (!init->expr)
    return new_node(ND_NUM, tok); // not given: already zeroed
  Node *lhs = init_desg_expr(desg, tok);
  return new_binary(ND_ASSIGN, lhs, init->expr, tok);
}

// A local with an initializer becomes: zero the whole variable, then assign the given parts.
// That is how "int a[100] = {1};" leaves a[1..99] as 0, as C requires.
static Node *lvar_initializer(Token **rest, Token *tok, Obj *var) {
  Initializer *init = initializer(rest, tok, var->ty, &var->ty);
  InitDesg desg = {.var = var};
  Node *lhs = new_node(ND_MEMZERO, tok);
  lhs->var = var;
  Node *rhs = create_lvar_init(init, var->ty, &desg, tok);
  return new_binary(ND_COMMA, lhs, rhs, tok);
}

// ---- global initializers: evaluated at compile time into bytes ---------------

static int64_t eval2(Node *node, char ***label);

static int64_t eval_rval(Node *node, char ***label) {
  switch (node->kind) {
  case ND_VAR:
    if (node->var->is_local)
      error_tok(node->tok, "not a compile-time constant");
    *label = &node->var->name;
    return 0;
  case ND_DEREF:
    return eval2(node->lhs, label);
  case ND_MEMBER:
    return eval_rval(node->lhs, label) + node->member->offset;
  default:
    error_tok(node->tok, "invalid initializer");
  }
}

// Evaluates a constant expression. If `label` is given, the result may be "address of a
// global symbol + offset", which the assembler resolves later (a relocation).
static int64_t eval2(Node *node, char ***label) {
  add_type(node);
  switch (node->kind) {
  case ND_ADD: return eval2(node->lhs, label) + eval2(node->rhs, NULL);
  case ND_SUB: return eval2(node->lhs, label) - eval2(node->rhs, NULL);
  case ND_MUL: return eval2(node->lhs, NULL) * eval2(node->rhs, NULL);
  case ND_DIV: {
    int64_t r = eval2(node->rhs, NULL);
    if (r == 0)
      error_tok(node->tok, "division by zero in constant expression");
    return eval2(node->lhs, NULL) / r;
  }
  case ND_MOD: {
    int64_t r = eval2(node->rhs, NULL);
    if (r == 0)
      error_tok(node->tok, "division by zero in constant expression");
    return eval2(node->lhs, NULL) % r;
  }
  case ND_NEG: return -eval2(node->lhs, NULL);
  case ND_BITAND: return eval2(node->lhs, NULL) & eval2(node->rhs, NULL);
  case ND_BITOR: return eval2(node->lhs, NULL) | eval2(node->rhs, NULL);
  case ND_BITXOR: return eval2(node->lhs, NULL) ^ eval2(node->rhs, NULL);
  case ND_SHL: return eval2(node->lhs, NULL) << eval2(node->rhs, NULL);
  case ND_SHR: return eval2(node->lhs, NULL) >> eval2(node->rhs, NULL);
  case ND_EQ: return eval2(node->lhs, NULL) == eval2(node->rhs, NULL);
  case ND_NE: return eval2(node->lhs, NULL) != eval2(node->rhs, NULL);
  case ND_LT: return eval2(node->lhs, NULL) < eval2(node->rhs, NULL);
  case ND_LE: return eval2(node->lhs, NULL) <= eval2(node->rhs, NULL);
  case ND_COND: return eval2(node->cond, NULL) ? eval2(node->then, label) : eval2(node->els, label);
  case ND_COMMA: return eval2(node->rhs, label);
  case ND_NOT: return !eval2(node->lhs, NULL);
  case ND_BITNOT: return ~eval2(node->lhs, NULL);
  case ND_LOGAND: return eval2(node->lhs, NULL) && eval2(node->rhs, NULL);
  case ND_LOGOR: return eval2(node->lhs, NULL) || eval2(node->rhs, NULL);
  case ND_CAST: {
    int64_t val = eval2(node->lhs, label);
    if (is_integer(node->ty)) {
      switch (node->ty->size) {
      case 1: return (int8_t)val;
      case 2: return (int16_t)val;
      case 4: return (int32_t)val;
      }
    }
    return val;
  }
  case ND_ADDR:
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    return eval_rval(node->lhs, label);
  case ND_MEMBER:
    if (!label || node->ty->kind != TY_ARRAY)
      error_tok(node->tok, "not a compile-time constant");
    return eval_rval(node->lhs, label) + node->member->offset;
  case ND_VAR:
    if (!label)
      error_tok(node->tok, "not a compile-time constant");
    if (node->var->ty->kind != TY_ARRAY && node->var->ty->kind != TY_FUNC)
      error_tok(node->tok, "not a compile-time constant (only addresses of arrays/functions)");
    *label = &node->var->name;
    return 0;
  case ND_DEREF:
    // &table[1][0]: table[1] is *(table + 1), an array, i.e. still just an address
    if (label && node->ty->kind == TY_ARRAY)
      return eval2(node->lhs, label);
    error_tok(node->tok, "not a compile-time constant");
  case ND_NUM: return node->val;
  default: error_tok(node->tok, "not a compile-time constant");
  }
}

static int64_t eval(Node *node) {
  return eval2(node, NULL);
}

int64_t const_expr(Token **rest, Token *tok) {
  Node *node = conditional(rest, tok);
  return eval(node);
}

static void write_buf(char *buf, uint64_t val, int sz) {
  switch (sz) {
  case 1: *buf = (char)val; return;
  case 2: *(uint16_t *)buf = (uint16_t)val; return;
  case 4: *(uint32_t *)buf = (uint32_t)val; return;
  case 8: *(uint64_t *)buf = val; return;
  }
}

static Relocation *write_gvar_data(Relocation *cur, Initializer *init, Type *ty, char *buf, int offset) {
  if (ty->kind == TY_ARRAY) {
    for (int i = 0; i < ty->array_len; i++)
      cur = write_gvar_data(cur, init->children[i], ty->base, buf, offset + ty->base->size * i);
    return cur;
  }
  if (ty->kind == TY_STRUCT) {
    int i = 0;
    for (Member *mem = ty->members; mem; mem = mem->next, i++)
      cur = write_gvar_data(cur, init->children[i], mem->ty, buf, offset + mem->offset);
    return cur;
  }
  if (!init->expr)
    return cur;
  char **label = NULL;
  int64_t val = eval2(init->expr, &label);
  if (!label) {
    write_buf(buf + offset, (uint64_t)val, ty->size);
    return cur;
  }
  if (ty->size != 8)
    error_tok(init->expr->tok, "an address can only initialize a pointer");
  Relocation *rel = calloc(1, sizeof(Relocation));
  rel->offset = offset;
  rel->label = *label;
  rel->addend = val;
  cur->next = rel;
  return cur->next;
}

static void gvar_initializer(Token **rest, Token *tok, Obj *var) {
  Initializer *init = initializer(rest, tok, var->ty, &var->ty);
  Relocation head = {0};
  char *buf = calloc(1, var->ty->size);
  write_gvar_data(&head, init, var->ty, buf, 0);
  var->init_data = buf;
  var->rel = head.next;
}

// ---- statements ------------------------------------------------------------------

// declaration = declspec (declarator ("=" initializer)? ("," declarator ("=" initializer)?)*)? ";"
static Node *declaration(Token **rest, Token *tok, Type *basety, VarAttr *attr) {
  Node head = {0};
  Node *cur = &head;
  int i = 0;
  while (!equal(tok, ";")) {
    if (i++ > 0) {
      if (!equal(tok, ","))
        error_tok(tok, "expected ',' or ';'");
      tok = tok->next;
    }
    Type *ty = declarator(&tok, tok, basety);
    if (ty->kind == TY_VOID)
      error_tok(ty->name, "variable declared void");
    if (ty->kind == TY_FUNC)
      error_tok(ty->name, "function declarations inside functions are not supported");
    if (attr && attr->is_static)
      error_tok(ty->name, "static local variables are not supported by jcc");
    Obj *var = new_lvar(get_ident(ty->name), ty);
    var->tok = ty->name;
    if (equal(tok, "=")) {
      Node *expr = lvar_initializer(&tok, tok->next, var);
      cur = cur->next = new_unary(ND_EXPR_STMT, expr, tok);
    }
    if (var->ty->size < 0)
      error_tok(ty->name, "variable '%s' has an incomplete type", var->name);
  }
  Node *node = new_node(ND_BLOCK, tok);
  node->body = head.next;
  *rest = tok->next;
  return node;
}

static Node *stmt(Token **rest, Token *tok) {
  if (equal(tok, "return")) {
    Node *node = new_node(ND_RETURN, tok);
    if (consume(rest, tok->next, ";")) {
      if (current_fn->ty->return_ty->kind != TY_VOID)
        error_tok(tok, "non-void function must return a value");
      return node;
    }
    Node *exp = expr(&tok, tok->next);
    *rest = skip(tok, ";");
    add_type(exp);
    if (current_fn->ty->return_ty->kind == TY_VOID)
      error_tok(exp->tok, "void function cannot return a value");
    node->lhs = new_cast(exp, current_fn->ty->return_ty);
    return node;
  }

  if (equal(tok, "if")) {
    Node *node = new_node(ND_IF, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(&tok, tok);
    if (equal(tok, "else"))
      node->els = stmt(&tok, tok->next);
    *rest = tok;
    return node;
  }

  if (equal(tok, "switch")) {
    Node *node = new_node(ND_SWITCH, tok);
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    Node *sw = current_switch;
    current_switch = node;
    char *brk = brk_label;
    brk_label = node->brk_label = new_unique_name();
    node->then = stmt(rest, tok);
    current_switch = sw;
    brk_label = brk;
    return node;
  }

  if (equal(tok, "case")) {
    if (!current_switch)
      error_tok(tok, "'case' outside of a switch");
    Node *node = new_node(ND_CASE, tok);
    node->case_val = const_expr(&tok, tok->next);
    for (Node *c = current_switch->case_next; c; c = c->case_next)
      if (c->case_val == node->case_val)
        error_tok(tok, "duplicate case value %ld", node->case_val);
    tok = skip(tok, ":");
    node->label = new_unique_name();
    // Register the case before parsing its statement, so "case 1: case 1:" is caught.
    node->case_next = current_switch->case_next;
    current_switch->case_next = node;
    node->lhs = stmt(rest, tok);
    return node;
  }

  if (equal(tok, "default")) {
    if (!current_switch)
      error_tok(tok, "'default' outside of a switch");
    if (current_switch->default_case)
      error_tok(tok, "multiple default labels in one switch");
    Node *node = new_node(ND_CASE, tok);
    tok = skip(tok->next, ":");
    node->label = new_unique_name();
    node->lhs = stmt(rest, tok);
    current_switch->default_case = node;
    return node;
  }

  if (equal(tok, "for")) {
    Node *node = new_node(ND_FOR, tok);
    tok = skip(tok->next, "(");
    enter_scope(); // for (int i = 0; ...) i is only visible inside the loop
    char *brk = brk_label;
    char *cont = cont_label;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();
    if (is_typename(tok)) {
      Type *basety = declspec(&tok, tok, NULL);
      node->init = declaration(&tok, tok, basety, NULL);
    } else {
      node->init = expr_stmt(&tok, tok);
    }
    if (!equal(tok, ";"))
      node->cond = expr(&tok, tok);
    tok = skip(tok, ";");
    if (!equal(tok, ")"))
      node->inc = expr(&tok, tok);
    tok = skip(tok, ")");
    node->then = stmt(rest, tok);
    leave_scope();
    brk_label = brk;
    cont_label = cont;
    return node;
  }

  if (equal(tok, "while")) {
    Node *node = new_node(ND_FOR, tok); // while (c) s  ==  for (; c; ) s
    tok = skip(tok->next, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    char *brk = brk_label;
    char *cont = cont_label;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();
    node->then = stmt(rest, tok);
    brk_label = brk;
    cont_label = cont;
    return node;
  }

  if (equal(tok, "do")) {
    Node *node = new_node(ND_DO, tok);
    char *brk = brk_label;
    char *cont = cont_label;
    brk_label = node->brk_label = new_unique_name();
    cont_label = node->cont_label = new_unique_name();
    node->then = stmt(&tok, tok->next);
    brk_label = brk;
    cont_label = cont;
    tok = skip(tok, "while");
    tok = skip(tok, "(");
    node->cond = expr(&tok, tok);
    tok = skip(tok, ")");
    *rest = skip(tok, ";");
    return node;
  }

  if (equal(tok, "break")) {
    if (!brk_label)
      error_tok(tok, "'break' outside of a loop or switch");
    Node *node = new_node(ND_BREAK, tok);
    node->label = brk_label;
    *rest = skip(tok->next, ";");
    return node;
  }

  if (equal(tok, "continue")) {
    if (!cont_label)
      error_tok(tok, "'continue' outside of a loop");
    Node *node = new_node(ND_CONTINUE, tok);
    node->label = cont_label;
    *rest = skip(tok->next, ";");
    return node;
  }

  if (equal(tok, "goto"))
    unsupported(tok);

  if (equal(tok, "{"))
    return compound_stmt(rest, tok->next);

  return expr_stmt(rest, tok);
}

// compound-stmt = (typedef | declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok) {
  Node *node = new_node(ND_BLOCK, tok);
  Node head = {0};
  Node *cur = &head;
  enter_scope();
  while (!equal(tok, "}")) {
    if (tok->kind == TK_EOF)
      error_tok(tok, "expected '}' before end of file");
    // A name followed by ':' could be a label; otherwise a known typedef starts a declaration.
    if (is_typename(tok) && !equal(tok->next, ":")) {
      VarAttr attr = {0};
      Type *basety = declspec(&tok, tok, &attr);
      if (attr.is_typedef) {
        // typedef inside a function
        for (int first = 1; !consume(&tok, tok, ";"); first = 0) {
          if (!first)
            tok = skip(tok, ",");
          Type *ty = declarator(&tok, tok, basety);
          push_scope(get_ident(ty->name))->type_def = ty;
        }
        continue;
      }
      cur = cur->next = declaration(&tok, tok, basety, &attr);
    } else {
      cur = cur->next = stmt(&tok, tok);
    }
    add_type(cur);
  }
  leave_scope();
  node->body = head.next;
  *rest = tok->next;
  return node;
}

// expr-stmt = expr? ";"
static Node *expr_stmt(Token **rest, Token *tok) {
  if (equal(tok, ";")) {
    *rest = tok->next;
    return new_node(ND_BLOCK, tok);
  }
  Node *node = new_node(ND_EXPR_STMT, tok);
  node->lhs = expr(&tok, tok);
  *rest = skip(tok, ";");
  return node;
}

// ---- expressions ---------------------------------------------------------------

static Node *expr(Token **rest, Token *tok) {
  Node *node = assign(&tok, tok);
  if (equal(tok, ","))
    return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);
  *rest = tok;
  return node;
}

// Converts "A op= B" into "tmp = &A, *tmp = *tmp op B" so that A is evaluated only once.
// (Matters for things like a[i++] += 1.)
static Node *to_assign(Node *binary) {
  add_type(binary->lhs);
  add_type(binary->rhs);
  Token *tok = binary->tok;
  if (binary->lhs->kind != ND_VAR && binary->lhs->kind != ND_DEREF && binary->lhs->kind != ND_MEMBER)
    error_tok(tok, "the left side of an assignment must be a variable, *pointer, a[i] or s.field");
  Obj *var = new_lvar("", pointer_to(binary->lhs->ty));
  Node *expr1 = new_binary(ND_ASSIGN, new_var_node(var, tok), new_unary(ND_ADDR, binary->lhs, tok), tok);
  Node *expr2 = new_binary(ND_ASSIGN,
                           new_unary(ND_DEREF, new_var_node(var, tok), tok),
                           new_binary(binary->kind,
                                      new_unary(ND_DEREF, new_var_node(var, tok), tok),
                                      binary->rhs, tok),
                           tok);
  return new_binary(ND_COMMA, expr1, expr2, tok);
}

static void check_lvalue(Node *node) {
  switch (node->kind) {
  case ND_VAR:
  case ND_DEREF:
  case ND_MEMBER:
    return;
  default:
    error_tok(node->tok, "expression is not assignable");
  }
}

static Node *assign(Token **rest, Token *tok) {
  Node *node = conditional(&tok, tok);

  if (equal(tok, "=")) {
    check_lvalue(node);
    return new_binary(ND_ASSIGN, node, assign(rest, tok->next), tok);
  }
  if (equal(tok, "+="))
    return to_assign(new_add(node, assign(rest, tok->next), tok));
  if (equal(tok, "-="))
    return to_assign(new_sub(node, assign(rest, tok->next), tok));
  struct { char *op; NodeKind kind; } ops[] = {
    {"*=", ND_MUL}, {"/=", ND_DIV}, {"%=", ND_MOD}, {"&=", ND_BITAND},
    {"|=", ND_BITOR}, {"^=", ND_BITXOR}, {"<<=", ND_SHL}, {">>=", ND_SHR},
  };
  for (size_t i = 0; i < sizeof(ops) / sizeof(*ops); i++)
    if (equal(tok, ops[i].op))
      return to_assign(new_binary(ops[i].kind, node, assign(rest, tok->next), tok));

  *rest = tok;
  return node;
}

static Node *conditional(Token **rest, Token *tok) {
  Node *cond = logor(&tok, tok);
  if (!equal(tok, "?")) {
    *rest = tok;
    return cond;
  }
  Node *node = new_node(ND_COND, tok);
  node->cond = cond;
  node->then = expr(&tok, tok->next);
  tok = skip(tok, ":");
  node->els = conditional(rest, tok);
  return node;
}

static Node *logor(Token **rest, Token *tok) {
  Node *node = logand(&tok, tok);
  while (equal(tok, "||")) {
    Token *start = tok;
    node = new_binary(ND_LOGOR, node, logand(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

static Node *logand(Token **rest, Token *tok) {
  Node *node = bitor(&tok, tok);
  while (equal(tok, "&&")) {
    Token *start = tok;
    node = new_binary(ND_LOGAND, node, bitor(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

static Node *bitor(Token **rest, Token *tok) {
  Node *node = bitxor(&tok, tok);
  while (equal(tok, "|")) {
    Token *start = tok;
    node = new_binary(ND_BITOR, node, bitxor(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

static Node *bitxor(Token **rest, Token *tok) {
  Node *node = bitand(&tok, tok);
  while (equal(tok, "^")) {
    Token *start = tok;
    node = new_binary(ND_BITXOR, node, bitand(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

static Node *bitand(Token **rest, Token *tok) {
  Node *node = equality(&tok, tok);
  while (equal(tok, "&")) {
    Token *start = tok;
    node = new_binary(ND_BITAND, node, equality(&tok, tok->next), start);
  }
  *rest = tok;
  return node;
}

static Node *equality(Token **rest, Token *tok) {
  Node *node = relational(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "==")) {
      node = new_binary(ND_EQ, node, relational(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "!=")) {
      node = new_binary(ND_NE, node, relational(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

// a > b is parsed as b < a, so codegen only needs < and <=.
static Node *relational(Token **rest, Token *tok) {
  Node *node = shift(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "<")) {
      node = new_binary(ND_LT, node, shift(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "<=")) {
      node = new_binary(ND_LE, node, shift(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, ">")) {
      node = new_binary(ND_LT, shift(&tok, tok->next), node, start);
      continue;
    }
    if (equal(tok, ">=")) {
      node = new_binary(ND_LE, shift(&tok, tok->next), node, start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *shift(Token **rest, Token *tok) {
  Node *node = add(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "<<")) {
      node = new_binary(ND_SHL, node, add(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, ">>")) {
      node = new_binary(ND_SHR, node, add(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

// In C, "+" means different things depending on types:
//   int + int        ordinary addition
//   pointer + int    p + n moves n ELEMENTS, i.e. n * sizeof(*p) bytes
//   pointer + ptr    an error
static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);
  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs, tok);
  if (lhs->ty->base && rhs->ty->base)
    error_tok(tok, "cannot add two pointers");
  if (!lhs->ty->base && rhs->ty->base) { // canonicalize "num + ptr" to "ptr + num"
    Node *tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }
  if (!lhs->ty->base || !is_integer(rhs->ty))
    error_tok(tok, "invalid operands to +");
  rhs = new_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, tok), tok);
  return new_binary(ND_ADD, lhs, rhs, tok);
}

//   ptr - int   moves back n elements
//   ptr - ptr   number of elements between them
static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);
  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs, tok);
  if (lhs->ty->base && is_integer(rhs->ty)) {
    rhs = new_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, tok), tok);
    add_type(rhs);
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = lhs->ty;
    return node;
  }
  if (lhs->ty->base && rhs->ty->base) {
    Node *node = new_binary(ND_SUB, lhs, rhs, tok);
    node->ty = ty_long;
    return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
  }
  error_tok(tok, "invalid operands to -");
}

static Node *add(Token **rest, Token *tok) {
  Node *node = mul(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "+")) {
      node = new_add(node, mul(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "-")) {
      node = new_sub(node, mul(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *mul(Token **rest, Token *tok) {
  Node *node = cast(&tok, tok);
  for (;;) {
    Token *start = tok;
    if (equal(tok, "*")) {
      node = new_binary(ND_MUL, node, cast(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "/")) {
      node = new_binary(ND_DIV, node, cast(&tok, tok->next), start);
      continue;
    }
    if (equal(tok, "%")) {
      node = new_binary(ND_MOD, node, cast(&tok, tok->next), start);
      continue;
    }
    *rest = tok;
    return node;
  }
}

static Node *cast(Token **rest, Token *tok) {
  if (equal(tok, "(") && is_typename(tok->next)) {
    Token *start = tok;
    Type *ty = typename_(&tok, tok->next);
    tok = skip(tok, ")");
    Node *node = new_cast(cast(rest, tok), ty);
    node->tok = start;
    return node;
  }
  return unary(rest, tok);
}

// Turns a++ into (typeof a)((a += 1) - 1): increment, then yield the OLD value.
static Node *new_inc_dec(Node *node, Token *tok, int addend) {
  add_type(node);
  return new_cast(new_add(to_assign(new_add(node, new_num(addend, tok), tok)),
                          new_num(-addend, tok), tok),
                  node->ty);
}

static Node *unary(Token **rest, Token *tok) {
  if (equal(tok, "+"))
    return cast(rest, tok->next);
  if (equal(tok, "-"))
    return new_unary(ND_NEG, cast(rest, tok->next), tok);
  if (equal(tok, "&")) {
    Node *lhs = cast(rest, tok->next);
    add_type(lhs);
    check_lvalue(lhs);
    return new_unary(ND_ADDR, lhs, tok);
  }
  if (equal(tok, "*")) {
    Node *lhs = cast(rest, tok->next);
    add_type(lhs);
    if (lhs->ty->kind == TY_FUNC)
      return lhs;
    return new_unary(ND_DEREF, lhs, tok);
  }
  if (equal(tok, "!"))
    return new_unary(ND_NOT, cast(rest, tok->next), tok);
  if (equal(tok, "~"))
    return new_unary(ND_BITNOT, cast(rest, tok->next), tok);
  // ++i is i += 1
  if (equal(tok, "++"))
    return to_assign(new_add(unary(rest, tok->next), new_num(1, tok), tok));
  if (equal(tok, "--"))
    return to_assign(new_sub(unary(rest, tok->next), new_num(1, tok), tok));
  if (equal(tok, "sizeof") && equal(tok->next, "(") && is_typename(tok->next->next)) {
    Type *ty = typename_(&tok, tok->next->next);
    *rest = skip(tok, ")");
    if (ty->size < 0)
      error_tok(tok, "sizeof an incomplete type");
    return new_long(ty->size, tok);
  }
  if (equal(tok, "sizeof")) {
    Node *node = unary(rest, tok->next);
    add_type(node); // compute the type without generating any code: sizeof(f()) never calls f
    return new_long(node->ty->size, tok);
  }
  return postfix(rest, tok);
}

static Node *postfix(Token **rest, Token *tok) {
  Node *node = primary(&tok, tok);
  for (;;) {
    if (equal(tok, "[")) {
      // a[i] is defined as *(a + i)
      Token *start = tok;
      Node *idx = expr(&tok, tok->next);
      tok = skip(tok, "]");
      node = new_unary(ND_DEREF, new_add(node, idx, start), start);
      continue;
    }
    if (equal(tok, ".")) {
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }
    if (equal(tok, "->")) {
      // p->x is (*p).x
      node = new_unary(ND_DEREF, node, tok);
      node = struct_ref(node, tok->next);
      tok = tok->next->next;
      continue;
    }
    if (equal(tok, "++")) {
      node = new_inc_dec(node, tok, 1);
      tok = tok->next;
      continue;
    }
    if (equal(tok, "--")) {
      node = new_inc_dec(node, tok, -1);
      tok = tok->next;
      continue;
    }
    *rest = tok;
    return node;
  }
}

// funcall = ident "(" (assign ("," assign)*)? ")"
static Node *funcall(Token **rest, Token *tok) {
  Token *start = tok;
  tok = tok->next->next;

  VarScope *sc = find_var(start);
  Type *ty;
  if (!sc) {
    // C89 "implicit declaration": assume int name(...). Warn, because it is usually a typo.
    warn_tok(start, "implicit declaration of function '%.*s'", start->len, start->loc);
    ty = func_type(ty_int);
    ty->is_variadic = true;
  } else {
    if (!sc->var || sc->var->ty->kind != TY_FUNC)
      error_tok(start, "'%.*s' is not a function", start->len, start->loc);
    ty = sc->var->ty;
  }

  Type *param_ty = ty->params;
  Node head = {0};
  Node *cur = &head;
  int nargs = 0;
  while (!equal(tok, ")")) {
    if (cur != &head)
      tok = skip(tok, ",");
    Node *arg = assign(&tok, tok);
    add_type(arg);
    if (arg->ty->kind == TY_STRUCT)
      error_tok(arg->tok, "passing structs by value is not supported by jcc; pass a pointer");
    if (param_ty) {
      arg = new_cast(arg, param_ty); // convert to the declared parameter type
      param_ty = param_ty->next;
    } else if (!ty->is_variadic) {
      error_tok(arg->tok, "too many arguments to function '%.*s'", start->len, start->loc);
    }
    cur = cur->next = arg;
    nargs++;
  }
  if (param_ty)
    error_tok(tok, "too few arguments to function '%.*s'", start->len, start->loc);
  *rest = skip(tok, ")");

  Node *node = new_node(ND_FUNCALL, start);
  node->funcname = strndup(start->loc, start->len);
  node->func_ty = ty;
  node->ty = ty->return_ty;
  node->args = head.next;
  return node;
}

static Node *primary(Token **rest, Token *tok) {
  if (equal(tok, "(")) {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }
  if (tok->kind == TK_IDENT) {
    if (equal(tok->next, "("))
      return funcall(rest, tok);
    VarScope *sc = find_var(tok);
    if (!sc || !sc->var)
      error_tok(tok, "undeclared variable '%.*s'", tok->len, tok->loc);
    *rest = tok->next;
    return new_var_node(sc->var, tok);
  }
  if (tok->kind == TK_STR) {
    Obj *var = new_string_literal(tok->str, tok->ty);
    *rest = tok->next;
    return new_var_node(var, tok);
  }
  if (tok->kind == TK_NUM) {
    Node *node = new_num(tok->val, tok);
    if (tok->ty)
      node->ty = tok->ty; // 1L is a long even though 1 fits in an int
    *rest = tok->next;
    return node;
  }
  error_tok(tok, "expected an expression");
}

// ---- top level -------------------------------------------------------------------

static Token *parse_typedef(Token *tok, Type *basety) {
  bool first = true;
  while (!consume(&tok, tok, ";")) {
    if (!first)
      tok = skip(tok, ",");
    first = false;
    Type *ty = declarator(&tok, tok, basety);
    push_scope(get_ident(ty->name))->type_def = ty;
  }
  return tok;
}

static void create_param_lvars(Type *param) {
  if (param) {
    create_param_lvars(param->next);
    if (!param->name)
      error("parameter name missing");
    new_lvar(get_ident(param->name), param);
  }
}

static Obj *find_func(char *name) {
  for (Obj *o = globals; o; o = o->next)
    if (o->is_function && !strcmp(o->name, name))
      return o;
  return NULL;
}

static Token *function(Token *tok, Type *basety) {
  Type *ty = declarator(&tok, tok, basety);
  char *name = get_ident(ty->name);
  if (ty->return_ty->kind == TY_STRUCT || ty->return_ty->kind == TY_ARRAY)
    error_tok(ty->name, "functions returning structs or arrays are not supported by jcc");

  Obj *fn = find_func(name);
  if (fn) {
    if (fn->is_definition && equal(tok, "{"))
      error_tok(ty->name, "redefinition of function '%s'", name);
    fn->ty = ty; // a later declaration (or the definition) replaces an earlier one
    push_scope(name)->var = fn;
  } else {
    fn = new_gvar(name, ty);
    fn->is_function = true;
  }
  fn->tok = ty->name;
  fn->is_definition = fn->is_definition || !consume(&tok, tok, ";");
  if (!fn->is_definition || fn->body)
    return tok;

  current_fn = fn;
  locals = NULL;
  enter_scope();
  create_param_lvars(ty->params);
  fn->params = locals;
  tok = skip(tok, "{");
  fn->body = compound_stmt(&tok, tok);
  fn->locals = locals;
  leave_scope();
  return tok;
}

static Token *global_variable(Token *tok, Type *basety) {
  bool first = true;
  while (!consume(&tok, tok, ";")) {
    if (!first) {
      if (!equal(tok, ","))
        error_tok(tok, "expected ',' or ';'");
      tok = tok->next;
    }
    first = false;
    Type *ty = declarator(&tok, tok, basety);
    if (ty->kind == TY_VOID)
      error_tok(ty->name, "variable declared void");
    Obj *var = new_gvar(get_ident(ty->name), ty);
    var->tok = ty->name;
    var->is_definition = true;
    if (equal(tok, "="))
      gvar_initializer(&tok, tok->next, var);
    if (var->ty->size < 0)
      error_tok(ty->name, "global '%s' has an incomplete type", var->name);
  }
  return tok;
}

// Peeks ahead: "int f(" starts a function, anything else is a global variable.
static bool is_function(Token *tok) {
  if (equal(tok, ";"))
    return false;
  Type dummy = {0};
  Type *ty = declarator(&tok, tok, &dummy);
  return ty->kind == TY_FUNC;
}

Obj *parse(Token *tok) {
  globals = NULL;
  while (tok->kind != TK_EOF) {
    VarAttr attr = {0};
    Type *basety = declspec(&tok, tok, &attr);
    if (attr.is_typedef) {
      tok = parse_typedef(tok, basety);
      continue;
    }
    if (equal(tok, ";")) { // a bare "struct point { ... };"
      tok = tok->next;
      continue;
    }
    if (is_function(tok)) {
      tok = function(tok, basety);
      continue;
    }
    tok = global_variable(tok, basety);
  }
  return globals;
}
