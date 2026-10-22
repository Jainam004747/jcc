// expect-error: cannot add two pointers
int main() { int a; int *p = &a; int *q = &a; p + q; return 0; }
