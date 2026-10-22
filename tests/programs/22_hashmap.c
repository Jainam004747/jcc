#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Separate-chaining hash map with string keys, compiled by jcc.
typedef struct entry Entry;
struct entry { char *key; int value; Entry *next; };
typedef struct { Entry **buckets; int nbuckets; int size; } Map;
long hash(char *s) { long h = 5381; while (*s) h = h * 33 + *s++; return h < 0 ? -h : h; }
void put(Map *m, char *key, int value) {
  long b = hash(key) % m->nbuckets;
  for (Entry *e = m->buckets[b]; e; e = e->next)
    if (strcmp(e->key, key) == 0) { e->value = value; return; }
  Entry *e = malloc(sizeof(Entry));
  e->key = key; e->value = value; e->next = m->buckets[b];
  m->buckets[b] = e;
  m->size++;
}
int get(Map *m, char *key, int fallback) {
  for (Entry *e = m->buckets[hash(key) % m->nbuckets]; e; e = e->next)
    if (strcmp(e->key, key) == 0) return e->value;
  return fallback;
}
int main() {
  Map m;
  m.nbuckets = 8;
  m.size = 0;
  m.buckets = calloc(8, sizeof(Entry *));
  char *words[] = {"coffee", "donut", "tea", "coffee", "bagel", "tea", "coffee"};
  for (int i = 0; i < 7; i++) put(&m, words[i], get(&m, words[i], 0) + 1);
  printf("size=%d coffee=%d tea=%d donut=%d muffin=%d\n", m.size, get(&m, "coffee", 0),
         get(&m, "tea", 0), get(&m, "donut", 0), get(&m, "muffin", -1));
  return 0;
}
