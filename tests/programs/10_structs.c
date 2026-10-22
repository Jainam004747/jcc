#include <stdio.h>
struct point { int x; int y; };
struct rect { struct point tl; struct point br; char name[8]; };
struct mixed { char c; long l; short s; };
int area(struct rect *r) { return (r->br.x - r->tl.x) * (r->br.y - r->tl.y); }
int main() {
  struct rect r = {{1, 2}, {5, 7}, "box"};
  printf("%d %s\n", area(&r), r.name);
  struct point p;
  p.x = 3; p.y = 4;
  struct point q = p;   // struct copy
  q.x = 100;
  printf("%d %d %d\n", p.x, q.x, q.y);
  printf("%ld %ld %ld\n", sizeof(struct point), sizeof(struct rect), sizeof(struct mixed));
  struct point pts[3] = {{1, 1}, {2, 4}, {3, 9}};
  int total = 0;
  for (int i = 0; i < 3; i++) total += pts[i].y;
  struct point *pp = pts;
  pp++;
  printf("%d %d\n", total, pp->y);
  return 0;
}
