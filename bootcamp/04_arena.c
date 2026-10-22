// An arena allocator: many small allocations carved out of big blocks, freed together.
// A compiler allocates thousands of tokens and AST nodes that all die together when it exits,
// which is exactly the case arenas are good at.
#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Block Block;
struct Block {
  Block *next;
  size_t used;
  size_t size;
  alignas(max_align_t) unsigned char data[];
};

typedef struct {
  Block *head;
} Arena;

static void *arena_alloc(Arena *a, size_t n) {
  n = (n + alignof(max_align_t) - 1) & ~(alignof(max_align_t) - 1); // keep every result aligned
  if (!a->head || a->head->used + n > a->head->size) {
    size_t size = n > 64 * 1024 ? n : 64 * 1024;
    Block *b = malloc(sizeof(Block) + size);
    assert(b);
    b->next = a->head;
    b->used = 0;
    b->size = size;
    a->head = b;
  }
  void *p = a->head->data + a->head->used;
  a->head->used += n;
  return memset(p, 0, n);
}

static void arena_free(Arena *a) {
  while (a->head) {
    Block *next = a->head->next;
    free(a->head);
    a->head = next;
  }
}

typedef struct Node {
  int kind;
  struct Node *lhs, *rhs;
} Node;

int main(void) {
  Arena arena = {0};
  Node *root = NULL;
  for (int i = 0; i < 100000; i++) { // build a long chain of nodes
    Node *n = arena_alloc(&arena, sizeof(Node));
    n->kind = i;
    n->lhs = root;
    root = n;
  }
  int count = 0;
  for (Node *n = root; n; n = n->lhs)
    count++;
  assert(count == 100000);
  arena_free(&arena); // one call frees all 100,000 nodes
  printf("04_arena: ok\n");
  return 0;
}
