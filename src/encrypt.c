char *__decrypt(char *encStr) {
  // 将原字符串保存到一个新的字符串中
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