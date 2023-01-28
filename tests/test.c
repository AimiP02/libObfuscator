#include <stdio.h>

void func(const char *s) {
  puts("!!!The testing string!!!");
  puts(s);
}

int main() {
  puts("This is a testing string!");
  char ch;
  if ((ch = getchar()) == '6') {
    printf("6666%c\n", ch);
  } else {
    printf("WTF?!\n");
  }
  func("!!!The testing string!!!");
  return 0;
}
