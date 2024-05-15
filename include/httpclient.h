#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "csocket-client.h"

#include "types.h"
#include "httpparser.h"

struct httpclient_request_params {
  char *host;
  int port;
  bool secure;
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
  struct csocket_client connection;
};

int httpclient_request(struct httpclient_request_params *request, struct httpclient_response *http_response);

void httpclient_shutdown(struct httpclient_response *response);

void httpclient_free(struct httpclient_response *response);

#endif
