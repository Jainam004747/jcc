// A growable array ("vector"): what jcc uses conceptually for lists of things.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int *data;
  size_t len; // elements in use
  size_t cap; // elements allocated
} Vec;

// Doubling the capacity when full makes push O(1) on average: n pushes copy at most 2n elements.
static void vec_push(Vec *v, int x) {
  if (v->len == v->cap) {
    size_t new_cap = v->cap ? v->cap * 2 : 4;
    int *bigger = realloc(v->data, new_cap * sizeof *bigger);
    if (!bigger) {
      perror("realloc");
      exit(1);
    }
    v->data = bigger;
    v->cap = new_cap;
  }
  v->data[v->len++] = x;
}

static int vec_pop(Vec *v) {
  assert(v->len > 0);
  return v->data[--v->len];
}

static void vec_free(Vec *v) {
  free(v->data);
  memset(v, 0, sizeof *v); // avoid a dangling pointer
}

int main(void) {
  Vec v = {0};
  for (int i = 0; i < 1000; i++)
    vec_push(&v, i);
  assert(v.len == 1000 && v.cap == 1024);
  assert(vec_pop(&v) == 999);
  assert(v.data[500] == 500);
  vec_free(&v);
  printf("02_vector: ok\n");
  return 0;
}
