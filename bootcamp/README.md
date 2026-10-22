# C bootcamp

Four short exercises that teach the C you need to read and change jcc. Each file is a small, complete, tested program. Build and run them all with AddressSanitizer:

```bash
make -C bootcamp test
```

| File | Concept | What it shows |
|---|---|---|
| `01_pointers.c` | Pointers and memory | Addresses, `*` and `&`, pointer arithmetic, arrays decaying to pointers, stack vs heap |
| `02_vector.c` | A growable array | `malloc`/`realloc`/`free`, capacity doubling (amortised O(1) push), ownership |
| `03_hashmap.c` | A hash map | Hashing strings (FNV-1a), open addressing with linear probing, resizing at 70% load |
| `04_arena.c` | An arena allocator | Allocating many small objects from one block and freeing them all at once. Compilers do this for tokens and AST nodes. |

## How to practise

1. Read the file top to bottom. The comments explain each step.
2. Delete one function's body and rewrite it from the comment above it.
3. Run `make -C bootcamp test`. AddressSanitizer will catch any out-of-bounds access, use-after-free or leak.
