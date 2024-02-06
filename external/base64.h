#ifndef BASE64_H
#define BASE64_H

size_t b64_encoded_size(size_t inlen);

char *b64_encode(const unsigned char *in, char *out, size_t len);

size_t b64_decoded_size(const char *in, size_t length);

int b64_decode(const char *in, unsigned char *out, size_t outlen);

#endif
