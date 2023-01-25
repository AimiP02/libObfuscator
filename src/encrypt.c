char *__decrypt(char *encStr) {
  char *curr = encStr;
  while (*curr) {
    *curr ^= 'a';
    curr++;
  }
  return encStr;
}

char *__encrypt(char *originStr) {
  char *curr = originStr;
  while (*curr) {
    *curr ^= 'a';
    curr++;
  }
  return originStr;
}