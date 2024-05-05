/*
  licenses/urldecode.license
*/

#include <stdlib.h>
#include <ctype.h>

size_t urldecoder_decode_length(const char *str) {
  size_t len = 0;
  char a, b;

  while (*str) {
    if ((*str == '%') && ((a = str[1]) && (b = str[2])) && (isxdigit(a) && isxdigit(b))) {
      len++;
      str += 3;
    } else if (*str == '+') {
      len++;
      str++;
    } else {
      len++;
      str++;
    }
  }

  return len;
}

void urldecoder_decode(char *dst, const char *src) {
  char a, b;

  while (*src) {
    if ((*src == '%') && ((a = src[1]) && (b = src[2])) && (isxdigit(a) && isxdigit(b))) {
      if (a >= 'a') a -= 'a'-'A';
      if (a >= 'A') a -= ('A' - 10);
      else a -= '0';

      if (b >= 'a') b -= 'a'-'A';
      if (b >= 'A') b -= ('A' - 10);
      else b -= '0';

      *dst++ = 16 * a + b;
      src += 3;
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }
}
