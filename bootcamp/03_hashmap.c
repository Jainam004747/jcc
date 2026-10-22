// A hash map with open addressing: keys and values live directly in one array.
// strdup() is POSIX, not ISO C. Without this line, -std=c11 hides its declaration, the compiler
// assumes it returns int, the 64-bit pointer is cut to 32 bits, and the program crashes.
// (This exact bug happened while writing this file; -Wall reports it as "implicit declaration".)
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char *key; // NULL = empty slot
  int value;
} Slot;

typedef struct {
  Slot *slots;
  size_t cap;
  size_t len;
} Map;

// FNV-1a: mixes every byte of the string into the hash.
static uint64_t hash(const char *s) {
  uint64_t h = 0xcbf29ce484222325ULL;
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 0x100000001b3ULL;
  }
  return h;
}

static void map_put(Map *m, const char *key, int value);

// When the table is 70% full, collisions get expensive: allocate a bigger one and reinsert.
static void grow(Map *m) {
  Map bigger = {calloc(m->cap ? m->cap * 2 : 8, sizeof(Slot)), m->cap ? m->cap * 2 : 8, 0};
  for (size_t i = 0; i < m->cap; i++) {
    if (m->slots[i].key) {
      map_put(&bigger, m->slots[i].key, m->slots[i].value);
      free(m->slots[i].key);
    }
  }
  free(m->slots);
  *m = bigger;
}

// Linear probing: start at hash % cap and walk forward until the key or an empty slot appears.
static void map_put(Map *m, const char *key, int value) {
  if (m->cap == 0 || (m->len + 1) * 10 > m->cap * 7)
    grow(m);
  for (size_t i = hash(key) % m->cap;; i = (i + 1) % m->cap) {
    Slot *s = &m->slots[i];
    if (!s->key) {
      s->key = strdup(key);
      s->value = value;
      m->len++;
      return;
    }
    if (strcmp(s->key, key) == 0) {
      s->value = value;
      return;
    }
  }
}

static int *map_get(Map *m, const char *key) {
  if (m->cap == 0)
    return NULL;
  for (size_t i = hash(key) % m->cap;; i = (i + 1) % m->cap) {
    Slot *s = &m->slots[i];
    if (!s->key)
      return NULL;
    if (strcmp(s->key, key) == 0)
      return &s->value;
  }
}

static void map_free(Map *m) {
  for (size_t i = 0; i < m->cap; i++)
    free(m->slots[i].key);
  free(m->slots);
}

int main(void) {
  Map m = {0};
  char key[32];
  for (int i = 0; i < 10000; i++) {
    snprintf(key, sizeof key, "key%d", i);
    map_put(&m, key, i * 2);
  }
  assert(m.len == 10000);
  assert(*map_get(&m, "key1234") == 2468);
  assert(map_get(&m, "missing") == NULL);
  map_put(&m, "key5", -1);
  assert(*map_get(&m, "key5") == -1 && m.len == 10000);
  map_free(&m);
  printf("03_hashmap: ok\n");
  return 0;
}
