# Design Decisions

### D-001: Scope: a C subset that runs real algorithms, not all of C
- **Context:** Full C (preprocessor, floats, unions, bitfields, the full type system) is several times larger.
- **Decision:** Signed integers, pointers, arrays, structs, typedefs and all statements and operators, enough to compile quicksort, a BST, a hash map and an interpreter. Everything else fails with a clear "not supported by jcc" error.
- **Trade-offs:** The system headers cannot be compiled, so a built-in prelude declares common libc functions.

### D-002: Generate assembly text, and let the system assembler and linker do the rest
- **Options:** Emit machine code and ELF files directly; emit assembly.
- **Decision:** Emit AT&T assembly and call `cc` to assemble and link.
- **Why:** The interesting part, turning C into instructions, is the same either way. Assembly text is readable, which matters for a learning project, and `cc` links libc correctly.

### D-003: Hand-written recursive-descent parser
- **Options:** A parser generator (yacc/bison), Pratt parsing, recursive descent.
- **Decision:** Recursive descent, one function per precedence level.
- **Why:** The grammar is visible in the code, error messages can be precise, and C's context-sensitive bits (typedef names vs variables, cast vs parenthesised expression) are easy to special-case. GCC and Clang also use hand-written recursive descent.

### D-004: Stack-machine code generation, no register allocation
- **Decision:** Every intermediate value goes through `%rax` and the stack.
- **Why:** Each AST node maps to a few obvious instructions, so the generated code is easy to verify by reading it.
- **Trade-offs:** Roughly 3–5× more instructions than `gcc -O2`. The next step would be an IR plus a register allocator.

### D-005: Keep every value sign-extended to 64 bits in `%rax`
- **Why:** One invariant makes loads, comparisons and widening casts simple. Narrowing casts just re-extend the low bits. Arithmetic on `int` still uses 32-bit instructions so overflow wraps exactly like C.

### D-006: Desugar in the parser
- **Decision:** `a[i]`, `->`, `>`, `>=`, `while`, compound assignment and `x++` are rewritten into a smaller set of node kinds.
- **Why:** The code generator handles fewer cases. Compound assignment through a temporary pointer guarantees the left side is evaluated once.

### D-007: Differential testing against gcc
- **Why:** gcc is the oracle. Any difference in output or exit code is either a jcc bug or undefined behaviour in the test, and both are worth knowing about.
- **Trade-offs:** Tests must avoid unspecified behaviour, such as the order of evaluation of function arguments.

### D-008: A built-in prelude instead of a preprocessor
- **Context:** Test programs use `#include <stdio.h>` so gcc can compile them too.
- **Decision:** jcc skips `#include` lines and parses a small prelude declaring common libc functions, with `long` standing in for `size_t`. Other directives are an error.
- **Trade-offs:** Not general, but the same source compiles under both gcc and jcc, which differential testing needs.

### D-009: Never free memory in the compiler
- **Why:** A compiler is a short-lived process, so the OS reclaims everything at exit. This is a deliberate, common choice (chibicc does the same); `make asan` runs with leak detection off for this reason. The bootcamp's arena allocator shows the production-grade alternative.
