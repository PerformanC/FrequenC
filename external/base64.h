/*
  License available on: licenses/base64.license

  Modifications:
    - Remove the need for malloc.
    - Added "len" parameter to b64_decode.
    - Removed NULL termination from b64_encode.

  Modified by: @ThePedroo
*/

#ifndef BASE64_H
#define BASE64_H

size_t b64_encoded_size(size_t inlen);

char *b64_encode(const unsigned char *in, char *out, size_t len);

size_t b64_decoded_size(const char *in, size_t length);

int b64_decode(const char *in, size_t len, unsigned char *out, size_t outlen);

#endif
