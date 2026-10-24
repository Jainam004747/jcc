# jcc

![CI](https://github.com/Jainam004747/jcc/actions/workflows/ci.yml/badge.svg)

jcc is a C compiler written from scratch in C. It compiles a useful subset of C to x86-64 assembly, then uses the system assembler and linker to produce a native Linux executable. It is about 3,100 lines with no dependencies.

I built it to understand what happens between `gcc hello.c` and a running program: tokenizing, parsing, type checking, and generating machine code that follows the real calling convention, so it can call the C library.

```
$ cat hello.c
#include <stdio.h>
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
int main() { printf("fib(20) = %d\n", fib(20)); return 0; }

$ ./jcc hello.c -o hello && ./hello
fib(20) = 6765
```

## What it compiles

| Area | Supported |
|---|---|
| Types | `char`, `short`, `int`, `long`, `void`, pointers (any depth), arrays (multi-dimensional), `struct` (nested, self-referential), `typedef` |
| Expressions | Every C operator: arithmetic, bitwise, shifts, comparisons, `&&` / `\|\|` (short-circuit), `?:`, comma, `sizeof`, casts, `++`/`--` (pre and post), all compound assignments, `&`, `*`, `[]`, `.`, `->` |
| Statements | `if`/`else`, `for` (with declarations), `while`, `do`/`while`, `switch`/`case`/`default` (with fall-through), `break`, `continue`, `return`, blocks with scoping |
| Functions | Recursion, mutual recursion, prototypes, more than 6 arguments (passed on the stack), variadic calls into libc such as `printf` |
| Data | Global and local initializers (`int a[] = {1,2,3}`, `char s[] = "hi"`, `struct p q = {1, 2}`), string literals with escapes, pointers to globals in initializers |
| Tooling | Error messages with line, column and a caret; `--dump-tokens`; `.loc` line info so `gdb` can step through C source |

Not supported (each gives a clear error): `unsigned`, `float`/`double`, `union`, `enum`, function pointers, `goto`, static locals, passing or returning structs by value, and the preprocessor beyond skipping `#include`. Common libc functions are declared by a built-in prelude instead.

## Quick start

```bash
make                         # build ./jcc
./jcc tests/programs/20_calculator.c -o calc && ./calc
./jcc -S tests/programs/06_functions.c -o - | less     # read the generated assembly
./jcc --dump-tokens tests/programs/01_return.c         # see what the tokenizer produces
make test                    # differential tests against gcc
make asan                    # the same tests with AddressSanitizer watching jcc itself
```

## How it works

```
source ──tokenize.c──▶ tokens ──parse.c──▶ AST ──type.c──▶ typed AST ──codegen.c──▶ x86-64 assembly ──cc──▶ executable
```

1. **Tokenizer** (`tokenize.c`): splits text into keywords, identifiers, numbers, strings and punctuators, decoding escapes like `\n` and `\x41`.
2. **Parser** (`parse.c`): a recursive-descent parser with one function per grammar rule, so operator precedence follows the call order. It resolves names through nested scopes and lays out structs with C's alignment rules.
3. **Type checker** (`type.c`): gives every expression a type and applies C's rules. For example, `p + 1` on an `int *` moves 4 bytes, and an array used as a value becomes a pointer.
4. **Code generator** (`codegen.c`): a stack machine. Each expression leaves its value in `%rax`; binary operators push one side and pop it back. It follows the System V AMD64 ABI, so jcc-compiled code can call `printf` and `malloc`.

For example, `int add(int a, int b) { return a + b; }` becomes:

```asm
add:
  push %rbp                 # prologue: new stack frame
  mov %rsp, %rbp
  sub $16, %rsp
  mov %edi, -4(%rbp)        # parameters arrive in registers; store them in their stack slots
  mov %esi, -8(%rbp)
  lea -8(%rbp), %rax        # evaluate b ...
  movslq (%rax), %rax
  push %rax                 # ... and save it on the stack
  lea -4(%rbp), %rax        # evaluate a
  movslq (%rax), %rax
  pop %rdi                  # bring b back
  add %edi, %eax            # 32-bit add: int arithmetic wraps like real C
  movslq %eax, %rax
  jmp .L.return.add
.L.return.add:
  mov %rbp, %rsp            # epilogue
  pop %rbp
  ret
```

More detail: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [docs/DECISIONS.md](docs/DECISIONS.md).

## Testing

`tests/run.sh` uses **differential testing**: each of the 32 programs in `tests/programs/` is compiled by gcc *and* by jcc, and both executables must print identical output and return the same exit code. The programs range from single features to real algorithms: quicksort, the sieve of Eratosthenes, N-queens, a binary search tree, a hash map, matrix multiplication, a recursive-descent calculator, and a Brainfuck interpreter.

The 15 programs in `tests/errors/` must be **rejected** with a specific message, for example `expected ',' or ';'` or `too few arguments to function`.

CI runs everything twice, once normally and once with jcc built with AddressSanitizer and UBSan, so memory bugs in the compiler itself fail the build.

## C bootcamp

[`bootcamp/`](bootcamp/README.md) holds the C fundamentals used to build jcc, each as a small tested program: pointers, a growable array, an open-addressing hash map, and an arena allocator.

## What I learned

- **How parsing turns precedence into structure.** In recursive descent, `a + b * c` parses correctly simply because `add()` calls `mul()`.
- **C's type rules are mostly about memory.** Pointer arithmetic scales by element size, arrays decay to pointers, and struct members are padded to their alignment.
- **The calling convention.** Which registers carry arguments, why `%rsp` must be 16-byte aligned at a `call`, and how variadic functions are told how many vector registers are used (`%al`).
- **Test against a reference.** Comparing against gcc caught real bugs, and it also showed that tests with unspecified evaluation order are themselves wrong.
