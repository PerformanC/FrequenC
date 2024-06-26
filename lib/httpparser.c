#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "libtstr.h"

#include "utils.h"

#include "httpparser.h"

void httpparser_init_request(struct httpparser_request *http_request, struct httpparser_header *buffer, int length) {
  http_request->path = NULL;
  http_request->headers_length = 0;
  http_request->headers_max_length = length;
  http_request->headers = buffer;
  http_request->body = NULL;
}

static int _httpparser_check_method(const char *method) {
  char *http_methods[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH" };

  for (size_t i = 0; i < sizeof(http_methods) / sizeof(*http_methods); i++) {
    if (strcmp(method, http_methods[i]) == 0) return 0;
  }

  return -1;
}

static void _httpparser_to_lower_case(char *str) {
  for (int i = 0; str[i]; i++) {
    str[i] = tolower(str[i]);
  }
}

int httpparser_parse_request(struct httpparser_request *http_request, const char *request, int request_length) {
  struct tstr_string_token headers_end;
  tstr_find_between(&headers_end, request, NULL, 0, "\r\n\r\n", 0);

  if (headers_end.end == 0) return -1;

  struct tstr_string_token method_token;
  tstr_find_between(&method_token, request, NULL, 0, " ", 0);

  if (method_token.end == 0) return -1;

  frequenc_fast_copy(request, http_request->method, (size_t)method_token.end);

  if (_httpparser_check_method(http_request->method) == -1) return -1;

  struct tstr_string_token path_token;
  tstr_find_between(&path_token, request, NULL, method_token.end + 1, " ", 0);

  if (path_token.end == 0) return -1;

  http_request->path = frequenc_safe_malloc((size_t)(path_token.end - path_token.start + 1) * sizeof(char));
  frequenc_fast_copy(request + path_token.start, http_request->path, (size_t)(path_token.end - path_token.start));

  struct tstr_string_token version_token;
  tstr_find_between(&version_token, request, NULL, path_token.end + 1 + (int)sizeof("HTTP/") - 1, "\r\n", 0);

  if (version_token.end == 0) return -1;

  frequenc_fast_copy(request + version_token.start, http_request->version, (size_t)(version_token.end - version_token.start));

  if (strcmp(http_request->version, "1.1")) return -1;

  int content_length = 0;
  struct tstr_string_token last_header = {
    .start = 0,
    .end = version_token.end
  };

  while (last_header.end != headers_end.end) {
    struct tstr_string_token header;
    tstr_find_between(&header, request, NULL, last_header.end + 2, "\r\n", 0);

    struct tstr_string_token header_name;
    tstr_find_between(&header_name, request, NULL, header.start, ": ", header.end);

    if (header_name.start == 0 || header_name.end == 0 || http_request->headers_length >= http_request->headers_max_length) return -1;

    int key_length = header_name.end - header_name.start;
    int value_length = header.end - header_name.end - 2;

    frequenc_fast_copy(request + header_name.start, http_request->headers[http_request->headers_length].key, (size_t)key_length);
    _httpparser_to_lower_case(http_request->headers[http_request->headers_length].key);
    frequenc_fast_copy(request + header_name.end + 2, http_request->headers[http_request->headers_length].value, (size_t)value_length);

    if (strcmp(http_request->headers[http_request->headers_length].key, "content-length") == 0)
      content_length = atoi(request + header.start + sizeof("content-length: ") - 1);

    last_header = header;

    http_request->headers_length++;
  }

  struct httpparser_header *transfer_encoding_header = httpparser_get_header(http_request, "transfer-encoding");

  if (content_length > 0 || transfer_encoding_header != NULL) {
    struct httpparser_header *content_type_header = httpparser_get_header(http_request, "content-type");
    if (content_type_header == NULL) return -1;

    if (transfer_encoding_header != NULL && strcmp(transfer_encoding_header->value, "chunked") == 0) {
      const char *body_and_length = request + headers_end.end + 4;

      struct tstr_string_token chunk_size;
      tstr_find_between(&chunk_size, body_and_length, NULL, 0, "\r\n", 0);

      if (chunk_size.end == 0) return -1;

      char chunk_size_str[5];
      frequenc_fast_copy(body_and_length, chunk_size_str, (size_t)chunk_size.end);

      http_request->chunk_length = (int)strtol(chunk_size_str, NULL, 16);

      int requested_length = (request_length + headers_end.end - 4) - chunk_size.end - 2;

      if (requested_length > http_request->chunk_length) requested_length = http_request->chunk_length;

      http_request->body_length = (size_t)requested_length;
      http_request->body = frequenc_safe_malloc((size_t)requested_length * sizeof(char));
      frequenc_unsafe_fast_copy(body_and_length + chunk_size.end + 2, http_request->body, (size_t)requested_length);

      /* TODO: Implement chunk handling */
      http_request->finished = http_request->body_length == (size_t)http_request->chunk_length;
    } else {
      if ((request_length - headers_end.end - 4) != content_length) return -1;

      http_request->body = frequenc_safe_malloc((size_t)content_length * sizeof(char));
      frequenc_unsafe_fast_copy(request + headers_end.end + 4, http_request->body, (size_t)content_length);

      http_request->body_length = (size_t)content_length;
      http_request->finished = true;
    }
  } else {
    if (content_length > 0) return -1;
  }

  return 0;
}

void httpparser_free_request(struct httpparser_request *http_request) {
  frequenc_safe_free(http_request->path);
  frequenc_safe_free(http_request->body);
}

void httpparser_init_response(struct httpparser_response *http_response, struct httpparser_header *buffer, int length) {
  http_response->status = 0;
  http_response->headers_length = 0;
  http_response->headers_max_length = length;
  http_response->headers = buffer;
  http_response->body = NULL;
}

static struct httpparser_header *_httpparser_get_response_header(struct httpparser_response *http_request, const char *key) {
  int i = 0;

  while (i < http_request->headers_length) {
    if (strcmp(http_request->headers[i].key, key) == 0) return &http_request->headers[i];

    i++;
  }

  return NULL;
}

int httpparser_parse_response(struct httpparser_response *http_response, const char *request, int request_length) {
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
  struct tstr_string_token last_header = {
    .start = 0,
    .end = reason.end
  };

  while (1) {
    struct tstr_string_token header;
    tstr_find_between(&header, request, NULL, last_header.end + 2, "\r\n", 0);

    struct tstr_string_token header_name;
    tstr_find_between(&header_name, request, NULL, header.start, ": ", header.end);

    if (header_name.start == 0 || header_name.end == 0) return -1;

    int key_length = header_name.end - header_name.start;
    int value_length = header.end - header_name.end - 2;

    frequenc_fast_copy(request + header_name.start, http_response->headers[i].key, (size_t)key_length);
    _httpparser_to_lower_case(http_response->headers[i].key);
    frequenc_fast_copy(request + header_name.end + 2, http_response->headers[i].value, (size_t)value_length);

    if (strcmp(http_response->headers[i].key, "content-length") == 0)
      content_length = atoi(request + header.start + sizeof("content-length: ") - 1);

    last_header = header;

    i++;

    if ((header.start == 0 || header.end == 0) || header.end == headers_end.end || i >= http_response->headers_max_length) break;
  }

  http_response->headers_length = i;

  struct httpparser_header *transfer_encoding_header = _httpparser_get_response_header(http_response, "transfer-encoding");

  if (transfer_encoding_header != NULL && strcmp(transfer_encoding_header->value, "chunked") == 0) {
    const char *body_and_length = request + headers_end.end + 4;

    struct tstr_string_token chunk_size;
    tstr_find_between(&chunk_size, body_and_length, NULL, 0, "\r\n", 0);

    if (chunk_size.end == 0) return -1;

    char chunk_size_str[5];
    frequenc_fast_copy(body_and_length, chunk_size_str, (size_t)chunk_size.end);

    http_response->chunk_length = (int)strtol(chunk_size_str, NULL, 16);
    http_response->body = frequenc_safe_malloc((size_t)http_response->chunk_length * sizeof(char));
    http_response->body_length = (size_t)(request_length - (headers_end.end + 4) - (chunk_size.end + 2));
    frequenc_unsafe_fast_copy(body_and_length + chunk_size.end + 2, http_response->body, http_response->body_length);
    http_response->finished = 0;
  } else {
    http_response->body_length = (size_t)(request_length - (headers_end.end + 4));

    if (http_response->body_length != (size_t)content_length) {
      frequenc_unsafe_free(http_response->body);

      return -1;
    }

    http_response->body = frequenc_safe_malloc((size_t)content_length * sizeof(char));
    frequenc_unsafe_fast_copy(request + headers_end.end + 4, http_response->body, (size_t)(request_length - headers_end.end - 4));

    http_response->body_length = (size_t)content_length;
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
