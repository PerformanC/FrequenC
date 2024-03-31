/*
  licenses/urldecode.license
*/

#ifndef URLDECODE_H
#define URLDECODE_H

size_t urldecoder_decode_length(const char *str);

void urldecoder_decode(char *dst, const char *src);

#endif
