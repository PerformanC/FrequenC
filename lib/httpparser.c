#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils.h"
#include "libtstr.h"

#include "httpparser.h"

void httpparser_init_request(struct httpparser_request *httpRequest, struct httpparser_header *buffer, int length) {
  httpRequest->path = NULL;
  httpRequest->headersLength = 0;
  httpRequest->headersMaxLength = length;
  httpRequest->headers = buffer;
  httpRequest->body = NULL;
}

int _httpparser_check_method(const char *method) {
  char *HTTPMethods[] = { "GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH" };

  for (size_t i = 0; i < sizeof(HTTPMethods) / sizeof(*HTTPMethods); i++) {
    if (strcmp(method, HTTPMethods[i]) == 0) return 0;
  }

  return -1;
}

int httpparser_parse_request(struct httpparser_request *httpRequest, const char *request) {
  struct tstr_string_token headersEnd;
  tstr_find_between(&headersEnd, request, NULL, 0, "\r\n\r\n", 0);

  if (headersEnd.end == 0) return -1;

  struct tstr_string_token methodToken;
  tstr_find_between(&methodToken, request, NULL, 0, " ", 0);

  if (methodToken.end == 0) return -1;

  frequenc_fast_copy((char *)request, httpRequest->method, methodToken.end);

  if (_httpparser_check_method(httpRequest->method) == -1) return -1;

  struct tstr_string_token pathToken;
  tstr_find_between(&pathToken, request, NULL, methodToken.end + 1, " ", 0);

  if (pathToken.end == 0) return -1;

  httpRequest->path = frequenc_safe_malloc((pathToken.end - pathToken.start + 1) * sizeof(char));
  frequenc_fast_copy((char *)request + pathToken.start, httpRequest->path, pathToken.end - pathToken.start);

  struct tstr_string_token versionToken;
  tstr_find_between(&versionToken, request, NULL, pathToken.end + 1 + sizeof("HTTP/") - 1, "\r\n", 0);

  if (versionToken.end == 0) return -1;

  frequenc_fast_copy((char *)request + versionToken.start, httpRequest->version, versionToken.end - versionToken.start);

  if (strcmp(httpRequest->version, "1.1")) return -1;

  int i = 0;
  int contentLength = 0;
  struct tstr_string_token lastHeader = {
    .start = 0,
    .end = 0
  };

  while (lastHeader.end != headersEnd.end) {
    struct tstr_string_token header;
    if (i == 0) {
      tstr_find_between(&header, request, NULL, versionToken.end + 2, "\r\n", 0);
    } else {
      tstr_find_between(&header, request, NULL, lastHeader.end + 2, "\r\n", 0);
    }

    struct tstr_string_token headerName;
    tstr_find_between(&headerName, request, NULL, header.start, ": ", header.end);

    if (headerName.start == 0 || headerName.end == 0) return -1;

    if (strstr(request + header.start, "Content-Length: ") == request + header.start) {
      contentLength = atoi(request + header.start + sizeof("Content-Length: ") - 1);
    }

    int keyLength = headerName.end - headerName.start;
    int valueLength = header.end - headerName.end - 2;

    frequenc_fast_copy((char *)request + headerName.start, httpRequest->headers[i].key, keyLength);
    frequenc_fast_copy((char *)request + headerName.end + 2, httpRequest->headers[i].value, valueLength);

    lastHeader = header;

    i++;
  }

  httpRequest->headersLength = i;

  if ((strcmp(httpRequest->method, "POST") == 0 || strcmp(httpRequest->method, "PUT") == 0) && contentLength > 0) {
    if (contentLength <= 0) return -1;

    struct httpparser_header *contentTypeHeader = httpparser_get_header(httpRequest, "Content-Type");
    if (contentTypeHeader == NULL) return -1;

    httpRequest->body = frequenc_safe_malloc((contentLength + 1) * sizeof(char));
    frequenc_fast_copy((char *)request + headersEnd.end + 4, httpRequest->body, contentLength);
  } else {
    if (contentLength > 0) return -1;
  }

  return 0;
}

void httpparser_free_request(struct httpparser_request *httpRequest) {
  if (httpRequest->path != NULL) free(httpRequest->path);
  if (httpRequest->body != NULL) free(httpRequest->body);
}

void httpparser_init_response(struct httpparser_response *httpResponse, struct httpparser_header *buffer, int length) {
  httpResponse->status = 0;
  httpResponse->headersLength = 0;
  httpResponse->headersMaxLength = length;
  httpResponse->headers = buffer;
  httpResponse->body = NULL;
}

struct httpparser_header *_httpparser_get_response_header(struct httpparser_response *httpRequest, const char *key) {
  int i = 0;

  while (i < httpRequest->headersLength) {
    if (strcmp(httpRequest->headers[i].key, key) == 0) return &httpRequest->headers[i];

    i++;
  }

  return NULL;
}

int httpparser_parse_response(struct httpparser_response *httpResponse, const char *request) {
  struct tstr_string_token headersEnd;
  tstr_find_between(&headersEnd, request, NULL, 0, "\r\n\r\n", 0);

  if (headersEnd.end == 0) return -1;

  struct tstr_string_token version;
  tstr_find_between(&version, request, "HTTP/", 0, " ", 0);

  if (version.start == 0 || version.end == 0) return -1;

  snprintf(httpResponse->version, sizeof(httpResponse->version), "%.*s", version.end - version.start, request + version.start);

  if (strcmp(httpResponse->version, "1.1")) return -1;

  struct tstr_string_token status;
  tstr_find_between(&status, request, NULL, version.end + 1, " ", 0);

  if (status.start == 0 || status.end == 0 || status.end - status.start != 3) return -1;

  char statusStr[4];
  snprintf(statusStr, sizeof(statusStr), "%.*s", status.end - status.start, request + status.start);
  httpResponse->status = atoi(statusStr);

  struct tstr_string_token reason;
  tstr_find_between(&reason, request, NULL, status.end + 1, "\r\n", 0);

  if (reason.start == 0 || reason.end == 0) return -1;

  snprintf(httpResponse->reason, sizeof(httpResponse->reason), "%.*s", reason.end - reason.start, request + reason.start);

  int i = 0;
  int contentLength = 0;
  struct tstr_string_token lastHeader;

  while (1) {
    struct tstr_string_token header;
    if (i == 0) {
      tstr_find_between(&header, request, NULL, reason.end + 2, "\r\n", 0);
    } else {
      tstr_find_between(&header, request, NULL, lastHeader.end + 2, "\r\n", 0);
    }

    struct tstr_string_token headerName;
    tstr_find_between(&headerName, request, NULL, header.start, ": ", header.end);

    if (headerName.start == 0 || headerName.end == 0) return -1;

    if (strstr(request + header.start, "Content-Length: ") == request + header.start) {
      contentLength = atoi(request + header.start + sizeof("Content-Length: ") - 1);
    }

    int keyLength = headerName.end - headerName.start;
    int valueLength = header.end - headerName.end - 2;

    snprintf(httpResponse->headers[i].key, sizeof(httpResponse->headers[i].key), "%.*s", keyLength, request + headerName.start);
    snprintf(httpResponse->headers[i].value, sizeof(httpResponse->headers[i].value), "%.*s", valueLength, request + headerName.end + 2);

    lastHeader = header;

    i++;

    if ((header.start == 0 || header.end == 0) || header.end == headersEnd.end || i >= httpResponse->headersMaxLength) break;
  }

  httpResponse->headersLength = i;

  struct httpparser_header *transferEncodingHeader = _httpparser_get_response_header(httpResponse, "Transfer-Encoding");

  if (transferEncodingHeader != NULL && strcmp(transferEncodingHeader->value, "chunked") == 0) {
    const char *bodyAndLength = request + headersEnd.end + 4;

    struct tstr_string_token chunkSize;
    tstr_find_between(&chunkSize, bodyAndLength, NULL, 0, "\r\n", 0);

    if (chunkSize.end == 0) return -1;

    char chunkSizeStr[5];
    frequenc_fast_copy((char *)bodyAndLength, chunkSizeStr, chunkSize.end);

    httpResponse->chunkLength = strtol(chunkSizeStr, NULL, 16);
    httpResponse->body = frequenc_safe_malloc((httpResponse->chunkLength + 1) * sizeof(char));
    httpResponse->bodyLength = snprintf(httpResponse->body, (httpResponse->chunkLength + 1), "%s", bodyAndLength + chunkSize.end + 2);
    httpResponse->finished = 0;
  } else {
    httpResponse->body = frequenc_safe_malloc((contentLength + 1) * sizeof(char));
    httpResponse->bodyLength = snprintf(httpResponse->body, (contentLength + 1), "%s", request + headersEnd.end + 4);

    if (httpResponse->bodyLength != (size_t)contentLength) {
      free(httpResponse->body);

      return -1;
    }

    httpResponse->bodyLength = contentLength;
    httpResponse->finished = 1;
  }

  return 0;
}

struct httpparser_header *httpparser_get_header(struct httpparser_request *httpRequest, const char *key) {
  int i = 0;

  while (i < httpRequest->headersLength) {
    if (strcmp(httpRequest->headers[i].key, key) == 0) return &httpRequest->headers[i];

    i++;
  }

  return NULL;
}
