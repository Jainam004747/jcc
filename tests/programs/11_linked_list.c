#include <stdio.h>
#include <stdlib.h>
typedef struct node Node;
struct node { int value; Node *next; };
Node *push(Node *head, int v) {
  Node *n = malloc(sizeof(Node));
  n->value = v;
  n->next = head;
  return n;
}
Node *reverse(Node *head) {
  Node *prev = 0;
  while (head) { Node *next = head->next; head->next = prev; prev = head; head = next; }
  return prev;
}
int main() {
  Node *list = 0;
  for (int i = 1; i <= 5; i++) list = push(list, i * i);
  list = reverse(list);
  for (Node *n = list; n; n = n->next) printf("%d ", n->value);
  printf("\n");
  while (list) { Node *next = list->next; free(list); list = next; }
  return 0;
}
