# jcc Architecture

## Files

| File | Lines (approx.) | Responsibility |
|---|---:|---|
| `src/jcc.h` | 220 | Every shared data structure: `Token`, `Node`, `Type`, `Member`, `Obj`. Start reading here. |
| `src/tokenize.c` | 380 | Source text → linked list of tokens; error reporting with line, column and caret |
| `src/parse.c` | 1,680 | Tokens → AST; scopes, declarations, structs, typedefs, initializers, constant folding |
| `src/type.c` | 180 | Type constructors and `add_type()`, which types every expression |
| `src/codegen.c` | 530 | AST → x86-64 assembly |
| `src/main.c` | 150 | Command-line driver, the libc prelude, and calling `cc` to assemble and link |

## Data structures

- **Token:** kind, position in the source (`loc`, `len`, `line_no`), and a value for numbers and strings. The parser walks a singly linked list of these.
- **Node:** one AST node. `kind` decides which fields are used: `lhs`/`rhs` for binary operators, `cond`/`then`/`els` for `if`, `init`/`cond`/`inc`/`then` for loops, `args` for calls. `ty` is filled in by `add_type()`.
- **Type:** `kind`, `size`, `align`, plus `base` (pointer or array element), `array_len`, `members` (struct) and `return_ty`/`params` (function).
- **Obj:** a variable or function. Locals have an `offset` from `%rbp`; globals have `init_data` bytes plus `rel` relocations for pointers inside initializers.

## Parsing

Recursive descent with **one function per precedence level** (the full grammar is at the top of `parse.c`):

```
expr → assign → conditional → logor → logand → bitor → bitxor → bitand
     → equality → relational → shift → add → mul → cast → unary → postfix → primary
```

Lower-precedence functions call higher-precedence ones, so the tree for `1 + 2 * 3` has `*` deeper than `+`, and it is computed first.

The parser also **desugars** constructs into simpler ones, so the code generator has less to handle:

| Source | Becomes |
|---|---|
| `a[i]` | `*(a + i)` |
| `p->x` | `(*p).x` |
| `a > b`, `a >= b` | `b < a`, `b <= a` |
| `while (c) s` | `for (; c; ) s` |
| `x += y` | `tmp = &x, *tmp = *tmp + y`, so `x` is evaluated once even for `a[i++] += 1` |
| `x++` | `(typeof x)((x += 1) - 1)` |
| `int a[3] = {1}` | zero `a`, then `a[0] = 1` |

**Scopes.** A stack of `Scope` objects. Each `{` pushes one, and a lookup walks from the innermost outwards, which implements shadowing. Struct tags live in a separate namespace from variables, as in C.

**Struct layout.** Each member is placed at the next offset that satisfies its alignment, and the total size is rounded up to the struct's largest alignment. So `struct { char c; long l; short s; }` has size 24, not 11.

**Constant expressions.** `eval2()` folds integer expressions at compile time, for array sizes, `case` labels and global initializers. For global pointers such as `char *p = "hi"` or `int *q = &table[1][0]`, it returns a *label + offset*, which becomes a `.quad label+offset` relocation for the assembler.

## Type rules (`type.c`)

- Integer operands smaller than `int` are promoted to `int`. If either side is `long`, the operation is `long`. Implicit conversions are inserted as `ND_CAST` nodes.
- `pointer ± integer` scales the integer by `sizeof(*pointer)`. `pointer − pointer` divides the byte difference by the element size.
- An array in an expression becomes a pointer to its first element. `sizeof` uses the type only and never evaluates its operand.

## Code generation (`codegen.c`)

**Model.** Every expression leaves its result in `%rax`. Binary operators evaluate the right side and push it, evaluate the left side, pop the right side into `%rdi`, and combine. Values are always kept **sign-extended to 64 bits**:

- loads of `char`/`short`/`int` use `movsbq`/`movswq`/`movslq`;
- arithmetic on `int` uses 32-bit instructions (so it wraps exactly as a C int does), then `movslq` restores the invariant;
- widening casts therefore need no instruction at all.

**Stack frames.** `assign_lvar_offsets` gives every local a slot below `%rbp`, aligned to its type. The frame size is rounded to 16 bytes.

**Calls (System V AMD64 ABI).**

1. Arguments are evaluated right to left and pushed.
2. The first six are popped into `rdi, rsi, rdx, rcx, r8, r9`; the rest stay on the stack.
3. If the push depth would leave `%rsp` misaligned, 8 bytes of padding are added first. The ABI requires 16-byte alignment at `call`.
4. `%eax` is set to 0, telling variadic functions like `printf` that no vector registers hold arguments.
5. The call goes through `@PLT`, so the result links into position-independent executables.
6. The return value is sign-extended according to the declared return type.

**Control flow** becomes labels and jumps. `switch` becomes a chain of `cmp`/`je` to each case label, then a jump to `default` or the end. `break`/`continue` jump to labels the parser created.

**Debugging.** Each statement and expression emits `.loc 1 <line>`, so `gdb` can map instructions back to C source lines.

## Testing strategy

| Test kind | Where | What it proves |
|---|---|---|
| Differential | `tests/programs/*.c` | Same output and exit status as gcc on 32 programs |
| Negative | `tests/errors/*.c` | Invalid code is rejected with the right message |
| Sanitizer | `make asan` | The compiler itself has no memory errors or undefined behaviour while compiling the suite |
