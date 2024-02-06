#ifndef HTTPPARSER_H
#define HTTPPARSER_H

#include <stdlib.h>

struct httpparser_header {
  char key[64];
  char value[1024];
};

struct httpparser_request {
  char method[7];
  char *path;
  char version[4];
  int headersLength;
  int headersMaxLength;
  struct httpparser_header *headers;
  char *body;
};

struct httpparser_response {
  char version[4];
  int status;
  char reason[32];
  int headersLength;
  int headersMaxLength;
  struct httpparser_header *headers;
  char *body;
  size_t bodySize;
  size_t bodyLength;
  int finished;
  int chunkLength;
};

void httpparser_init_request(struct httpparser_request *httpRequest, struct httpparser_header *buffer, int length);

int httpparser_parse_request(struct httpparser_request *httpRequest, const char *request);

void httpparser_free_request(struct httpparser_request *httpRequest);

void httpparser_init_response(struct httpparser_response *httpResponse, struct httpparser_header *buffer, int length);

int httpparser_parse_response(struct httpparser_response *httpResponse, const char *request);

struct httpparser_header *httpparser_get_header(struct httpparser_request *httpRequest, const char *key);

#endif
