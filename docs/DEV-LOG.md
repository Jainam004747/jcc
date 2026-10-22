# Development Log

## 2026-10-21 - Building jcc

### Environment
- gcc 13 and GNU binutils (the `as` assembler and `ld` linker) on x86-64 Linux. jcc writes assembly and lets `cc` assemble and link it with the C library.
- No external libraries. The compiler is plain C11 with GNU extensions (`open_memstream`).

### Build order (one commit each)
1. `bootcamp/`: pointers, a growable array, a hash map and an arena, all run under AddressSanitizer.
2. `src/jcc.h`: every data structure (Token, Node, Type, Obj) decided up front.
3. `src/tokenize.c`: tokens with line numbers, escapes, char and string literals, and error messages with a caret.
4. `src/type.c` + `src/parse.c`: recursive descent with full operator precedence, scopes, structs, typedefs, initializers, and compile-time constant evaluation.
5. `src/codegen.c`: stack-machine code generation, System V calling convention, stack arguments.
6. `tests/`: differential testing against gcc.

### Commands
```bash
make                 # build ./jcc
make test            # 32 programs compared against gcc, plus 15 must-fail programs
make asan            # same tests with AddressSanitizer + UBSan inside jcc itself
./jcc -S prog.c -o prog.s && cat prog.s    # read the generated assembly
./jcc --dump-tokens prog.c                 # see the tokenizer's output
```

### Bugs found by tests
- **`1L << 40` gave 256.** The `L` suffix was ignored, so `1L` was typed `int` and the shift used 32-bit registers. Fix: the tokenizer records the suffix, and the literal becomes `long`.
- **`&table[1][0]` in a global initializer was rejected.** The constant evaluator did not understand that `table[1]` (an array) is just an address. Fix: handle `ND_DEREF` of array type in `eval2`.
- **`case 1: case 1:` was accepted.** The case was registered *after* parsing its statement, so the nested duplicate was checked against an empty list. Fix: register first, then parse.
- **Tests with undefined behaviour.** `printf("%d %d", x, x++)` gave different output under gcc and jcc. That is correct, because C does not define the order of evaluation. Lesson: differential tests must avoid unspecified and undefined behaviour.
- **Bootcamp crash.** `strdup` under `-std=c11` had no declaration, so it was implicitly declared as returning `int` and the pointer was truncated to 32 bits. Fix: `#define _POSIX_C_SOURCE 200809L`.

### First optimisation
Casts from a smaller to a larger integer type emitted `movslq` even though values are always kept sign-extended. Skipping them removed about 15% of the instructions in typical functions.
