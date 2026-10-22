// expect-error: no member named 'z'
struct p { int x; };
int main() { struct p a; return a.z; }
