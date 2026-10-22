#include <stdio.h>
#include <stdlib.h>
typedef struct tree Tree;
struct tree { int key; Tree *left; Tree *right; };
Tree *insert(Tree *t, int key) {
  if (!t) { t = calloc(1, sizeof(Tree)); t->key = key; return t; }
  if (key < t->key) t->left = insert(t->left, key);
  else if (key > t->key) t->right = insert(t->right, key);
  return t;
}
int height(Tree *t) {
  if (!t) return 0;
  int l = height(t->left), r = height(t->right);
  return 1 + (l > r ? l : r);
}
void inorder(Tree *t) { if (t) { inorder(t->left); printf("%d ", t->key); inorder(t->right); } }
int main() {
  Tree *root = 0;
  int keys[] = {50, 30, 70, 20, 40, 60, 80, 35, 45, 65};
  for (int i = 0; i < 10; i++) root = insert(root, keys[i]);
  inorder(root);
  printf("\nheight %d\n", height(root));
  return 0;
}
