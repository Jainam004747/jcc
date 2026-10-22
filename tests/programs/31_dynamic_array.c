#include <stdio.h>
#include <stdlib.h>
typedef struct { int *data; int len; int cap; } Vec;
void vec_push(Vec *v, int x) {
  if (v->len == v->cap) {
    v->cap = v->cap ? v->cap * 2 : 4;
    v->data = realloc(v->data, v->cap * sizeof(int));
  }
  v->data[v->len++] = x;
}
int main() {
  Vec v = {0, 0, 0};
  for (int i = 0; i < 1000; i++) vec_push(&v, i * i);
  long sum = 0;
  for (int i = 0; i < v.len; i++) sum += v.data[i];
  printf("len=%d cap=%d sum=%ld last=%d\n", v.len, v.cap, sum, v.data[999]);
  free(v.data);
  return 0;
}
