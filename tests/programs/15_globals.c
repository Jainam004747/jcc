#include <stdio.h>
int counter;
long big = 1L << 40;
char letter = 'z';
char *greeting = "hi there";
char *names[] = {"ann", "bob", "cid"};
int table[2][3] = {{1, 2, 3}, {4, 5, 6}};
struct cfg { int port; char *host; } config = {6380, "localhost"};
int *ptr_to_table = &table[1][0];
void bump() { counter++; }
int main() {
  bump(); bump(); bump();
  printf("%d %ld %c %s\n", counter, big, letter, greeting);
  printf("%s %s %d %d\n", names[2], names[0], table[1][2], *ptr_to_table);
  printf("%s:%d\n", config.host, config.port);
  return 0;
}
