#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "types.h"
#include "httpparser.h"

#include <openssl/ssl.h>

struct httpclient_request_params {
  char *host;
  int port;
  char *method;
  char *path;
  int headers_length;
  struct httpparser_header *headers;
  struct tstr_string *body;
};

struct httpclient_response {
  char version[4];
  int status;
  char reason[32];
  int headers_length;
  int headers_max_length;
  struct httpparser_header *headers;
  char *body;
  size_t body_length;
  int finished;
  int chunk_length;
  SSL *ssl;
  SSL_CTX *ctx;
  int socket;
};

int httpclient_request(struct httpclient_request_params *request, struct httpclient_response *httpResponse);

int httpclient_unsafe_request(struct httpclient_request_params *request, struct httpclient_response *httpResponse);

void httpclient_shutdown(struct httpclient_response *response);

void httpclient_free(struct httpclient_response *response);

#endif
