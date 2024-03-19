#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"
#include "libtstr.h"

#include "httpparser.h"

void httpparser_init_request(struct httpparser_request *http_request, struct httpparser_header *buffer, int length) {
  http_request->path = NULL;
  http_request->headers_length = 0;
  http_request->headers_max_length = length;
  http_request->headers = buffer;
  http_request->body = NULL;
}

int _httpparser_check_method(const char *method) {
  char *http_methods[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH" };

  for (size_t i = 0; i < sizeof(http_methods) / sizeof(*http_methods); i++) {
    if (strcmp(method, http_methods[i]) == 0) return 0;
  }

  return -1;
}

int httpparser_parse_request(struct httpparser_request *http_request, const char *request) {
  struct tstr_string_token headers_end;
  tstr_find_between(&headers_end, request, NULL, 0, "\r\n\r\n", 0);

  if (headers_end.end == 0) return -1;

  struct tstr_string_token method_token;
  tstr_find_between(&method_token, request, NULL, 0, " ", 0);

  if (method_token.end == 0) return -1;

  frequenc_fast_copy((char *)request, http_request->method, method_token.end);

  if (_httpparser_check_method(http_request->method) == -1) return -1;

  struct tstr_string_token path_token;
  tstr_find_between(&path_token, request, NULL, method_token.end + 1, " ", 0);

  if (path_token.end == 0) return -1;

  http_request->path = frequenc_safe_malloc((path_token.end - path_token.start + 1) * sizeof(char));
  frequenc_fast_copy((char *)request + path_token.start, http_request->path, path_token.end - path_token.start);

  struct tstr_string_token version_token;
  tstr_find_between(&version_token, request, NULL, path_token.end + 1 + sizeof("HTTP/") - 1, "\r\n", 0);

  if (version_token.end == 0) return -1;

  frequenc_fast_copy((char *)request + version_token.start, http_request->version, version_token.end - version_token.start);

  if (strcmp(http_request->version, "1.1")) return -1;

  int i = 0;
  int content_length = 0;
  struct tstr_string_token last_header = {
    .start = 0,
    .end = 0
  };

  while (last_header.end != headers_end.end) {
    struct tstr_string_token header;
    if (i == 0) {
      tstr_find_between(&header, request, NULL, version_token.end + 2, "\r\n", 0);
    } else {
      tstr_find_between(&header, request, NULL, last_header.end + 2, "\r\n", 0);
    }

    struct tstr_string_token header_name;
    tstr_find_between(&header_name, request, NULL, header.start, ": ", header.end);

    if (header_name.start == 0 || header_name.end == 0) return -1;

    if (strstr(request + header.start, "Content-Length: ") == request + header.start) {
      content_length = atoi(request + header.start + sizeof("Content-Length: ") - 1);
    }

    int key_length = header_name.end - header_name.start;
    int value_length = header.end - header_name.end - 2;

    frequenc_fast_copy((char *)request + header_name.start, http_request->headers[i].key, key_length);
    frequenc_fast_copy((char *)request + header_name.end + 2, http_request->headers[i].value, value_length);

    last_header = header;

    i++;
  }

  http_request->headers_length = i;

  if ((strcmp(http_request->method, "POST") == 0 || strcmp(http_request->method, "PUT") == 0) && content_length > 0) {
    if (content_length <= 0) return -1;

    struct httpparser_header *content_type_header = httpparser_get_header(http_request, "Content-Type");
    if (content_type_header == NULL) return -1;

    http_request->body = frequenc_safe_malloc((content_length + 1) * sizeof(char));
    frequenc_fast_copy((char *)request + headers_end.end + 4, http_request->body, content_length);
  } else {
    if (content_length > 0) return -1;
  }

  return 0;
}

void httpparser_free_request(struct httpparser_request *http_request) {
  if (http_request->path != NULL) free(http_request->path);
  if (http_request->body != NULL) free(http_request->body);
}

void httpparser_init_response(struct httpparser_response *http_response, struct httpparser_header *buffer, int length) {
  http_response->status = 0;
  http_response->headers_length = 0;
  http_response->headers_max_length = length;
  http_response->headers = buffer;
  http_response->body = NULL;
}

struct httpparser_header *_httpparser_get_response_header(struct httpparser_response *http_request, const char *key) {
  int i = 0;

  while (i < http_request->headers_length) {
    if (strcmp(http_request->headers[i].key, key) == 0) return &http_request->headers[i];

    i++;
  }

  return NULL;
}

int httpparser_parse_response(struct httpparser_response *http_response, const char *request) {
  struct tstr_string_token headers_end;
  tstr_find_between(&headers_end, request, NULL, 0, "\r\n\r\n", 0);

  if (headers_end.end == 0) return -1;

  struct tstr_string_token version;
  tstr_find_between(&version, request, "HTTP/", 0, " ", 0);

  if (version.start == 0 || version.end == 0) return -1;

  snprintf(http_response->version, sizeof(http_response->version), "%.*s", version.end - version.start, request + version.start);

  if (strcmp(http_response->version, "1.1")) return -1;

  struct tstr_string_token status;
  tstr_find_between(&status, request, NULL, version.end + 1, " ", 0);

  if (status.start == 0 || status.end == 0 || status.end - status.start != 3) return -1;

  char status_str[4];
  snprintf(status_str, sizeof(status_str), "%.*s", status.end - status.start, request + status.start);
  http_response->status = atoi(status_str);

  struct tstr_string_token reason;
  tstr_find_between(&reason, request, NULL, status.end + 1, "\r\n", 0);

  if (reason.start == 0 || reason.end == 0) return -1;

  snprintf(http_response->reason, sizeof(http_response->reason), "%.*s", reason.end - reason.start, request + reason.start);

  int i = 0;
  int content_length = 0;
  struct tstr_string_token last_header;

  while (1) {
    struct tstr_string_token header;
    if (i == 0) {
      tstr_find_between(&header, request, NULL, reason.end + 2, "\r\n", 0);
    } else {
      tstr_find_between(&header, request, NULL, last_header.end + 2, "\r\n", 0);
    }

    struct tstr_string_token header_name;
    tstr_find_between(&header_name, request, NULL, header.start, ": ", header.end);

    if (header_name.start == 0 || header_name.end == 0) return -1;

    if (strstr(request + header.start, "Content-Length: ") == request + header.start) {
      content_length = atoi(request + header.start + sizeof("Content-Length: ") - 1);
    }

    int key_length = header_name.end - header_name.start;
    int value_length = header.end - header_name.end - 2;

    snprintf(http_response->headers[i].key, sizeof(http_response->headers[i].key), "%.*s", key_length, request + header_name.start);
    snprintf(http_response->headers[i].value, sizeof(http_response->headers[i].value), "%.*s", value_length, request + header_name.end + 2);

    last_header = header;

    i++;

    if ((header.start == 0 || header.end == 0) || header.end == headers_end.end || i >= http_response->headers_max_length) break;
  }

  http_response->headers_length = i;

  struct httpparser_header *transfer_encoding_header = _httpparser_get_response_header(http_response, "Transfer-Encoding");

  if (transfer_encoding_header != NULL && strcmp(transfer_encoding_header->value, "chunked") == 0) {
    const char *body_and_length = request + headers_end.end + 4;

    struct tstr_string_token chunk_size;
    tstr_find_between(&chunk_size, body_and_length, NULL, 0, "\r\n", 0);

    if (chunk_size.end == 0) return -1;

    char chunk_size_str[5];
    frequenc_fast_copy((char *)body_and_length, chunk_size_str, chunk_size.end);

    http_response->chunk_length = strtol(chunk_size_str, NULL, 16);
    http_response->body = frequenc_safe_malloc((http_response->chunk_length + 1) * sizeof(char));
    http_response->body_length = snprintf(http_response->body, (http_response->chunk_length + 1), "%s", body_and_length + chunk_size.end + 2);
    http_response->finished = 0;
  } else {
    http_response->body = frequenc_safe_malloc((content_length + 1) * sizeof(char));
    http_response->body_length = snprintf(http_response->body, (content_length + 1), "%s", request + headers_end.end + 4);

    if (http_response->body_length != (size_t)content_length) {
      free(http_response->body);

      return -1;
    }

    http_response->body_length = content_length;
    http_response->finished = 1;
  }

  return 0;
}

struct httpparser_header *httpparser_get_header(struct httpparser_request *http_request, const char *key) {
  int i = 0;

  while (i < http_request->headers_length) {
    if (strcmp(http_request->headers[i].key, key) == 0) return &http_request->headers[i];

    i++;
  }

  return NULL;
}
