#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcplimits.h"
#include "csocket-client.h"

#include "httpparser.h"
#include "libtstr.h"
#include "utils.h"

#include "httpclient.h"

static size_t _httpclient_calculate_request_length(struct httpclient_request_params *request) {
  size_t length = 0;

  length += sizeof("HTTP/1.1 ") - 1;
  length += strlen(request->method) + 1;
  length += strlen(request->path) + 1;
  length += (sizeof("\r\nHost: \r\n") - 1) + strlen(request->host);

  for (int i = 0; i < request->headers_length; i++) {
    length += strlen(request->headers[i].key);
    length += strlen(request->headers[i].value);

    length += 4;
  }

  if (request->body != NULL) {
    length += (sizeof("Content-Length: \r\n\r\n") - 1) + (size_t)snprintf(NULL, 0, "%zu", request->body->length) + request->body->length;
  } else {
    length += 2;
  }

  return length;
}

static void _httpclient_build_request(struct httpclient_request_params *request, struct tstr_string *response) {
  size_t length = _httpclient_calculate_request_length(request);
  size_t current_length = 0;
  char *request_string = frequenc_safe_malloc(length * sizeof(char));

  current_length = (size_t)snprintf(request_string, length, "%s %s HTTP/1.1\r\nHost: %s\r\n", request->method, request->path, request->host);

  for (int i = 0; i < request->headers_length; i++) {
    current_length += (size_t)snprintf(request_string + current_length, (length + 1) - current_length, "%s: %s\r\n", request->headers[i].key, request->headers[i].value);
  }

  if (request->body != NULL) {
    tstr_append(request_string, "Content-Length: ", &current_length, 16);

    char content_length_str[16];
    int content_length_length = snprintf(content_length_str, 16, "%zu", request->body->length);
    tstr_append(request_string, content_length_str, &current_length, content_length_length);

    tstr_append(request_string, "\r\n\r\n", &current_length, 4);
    tstr_append(request_string, request->body->string, &current_length, (int)request->body->length);
  } else {
    tstr_append(request_string, "\r\n", &current_length, 2);
  }

  response->string = request_string;
  response->length = length - 1;
}

int httpclient_request(struct httpclient_request_params *request, struct httpclient_response *http_response) {
  pcll_init_ssl_library();

  int ret = csocket_client_init(&http_response->connection, request->secure, request->host, request->port);
  if (ret == CSOCKET_CLIENT_ERROR) {
    perror("[https-client]: Failed to initialize connection");

    return -1;
  }

  struct tstr_string http_request;
  _httpclient_build_request(request, &http_request);

  ret = csocket_client_send(&http_response->connection, http_request.string, http_request.length);
  if (ret == CSOCKET_CLIENT_ERROR) {
    csocket_client_close(&http_response->connection);

    perror("[https-client]: Failed to send HTTP request");

    return -1;
  }
  frequenc_unsafe_free(http_request.string);

  char packet[TCPLIMITS_PACKET_SIZE];

  long len = csocket_client_recv(&http_response->connection, packet, TCPLIMITS_PACKET_SIZE);
  if (len == CSOCKET_CLIENT_ERROR) {
    csocket_client_close(&http_response->connection);

    perror("[https-client]: Failed to receive HTTP response");

    return -1;
  }

  struct httpparser_header headers[30];
  httpparser_init_response((struct httpparser_response *)http_response, headers, 30);

  if (httpparser_parse_response((struct httpparser_response *)http_response, packet, (int)len) == -1) {
    csocket_client_close(&http_response->connection);

    printf("[https-client]: Failed to parse HTTP response.\n");

    return -1;
  }

  if (http_response->finished == 0) {
    long chunk_size_left = (long)((size_t)http_response->chunk_length - http_response->body_length);
    
    read_chunk:

    http_response->body = realloc(http_response->body, http_response->body_length + (size_t)chunk_size_left + 1);

    if (http_response->body == NULL) {
      csocket_client_close(&http_response->connection);

      perror("[https-client]: Failed to allocate memory for response");

      return -1;
    }

    while (chunk_size_left > 0) {
      len = csocket_client_recv(&http_response->connection, packet, (size_t)(chunk_size_left > TCPLIMITS_PACKET_SIZE ? TCPLIMITS_PACKET_SIZE : chunk_size_left));
      if (len == CSOCKET_CLIENT_ERROR) {
        csocket_client_close(&http_response->connection);

        perror("[https-client]: Failed to receive HTTP response");

        return -1;
      }

      tstr_append(http_response->body, packet, &http_response->body_length, (int)len);
      chunk_size_left -= len;

      if (chunk_size_left == 0) break;
    }

    len = csocket_client_recv(&http_response->connection, packet, 2);
    if (len == CSOCKET_CLIENT_ERROR) {
      csocket_client_close(&http_response->connection);

      perror("[https-client]: Failed to receive HTTP response");

      return -1;
    }

    char chunk_size_str[5];
    int i = 0;
    while (1) {
      len = csocket_client_recv(&http_response->connection, packet, 1);
      if (len == CSOCKET_CLIENT_ERROR) {
        csocket_client_close(&http_response->connection);

        perror("[https-client]: Failed to receive HTTP response");

        return -1;
      }

      if (packet[0] == '\r') break;

      chunk_size_str[i] = packet[0];
      i++;
    }

    chunk_size_str[i] = '\0';

    chunk_size_left = strtol(chunk_size_str, NULL, 16);
    if (chunk_size_left == 0) return 0;

    len = csocket_client_recv(&http_response->connection, packet, 1);
    if (len == CSOCKET_CLIENT_ERROR) {
      csocket_client_close(&http_response->connection);

      perror("[https-client]: Failed to receive HTTP response");

      return -1;
    }

    goto read_chunk;
  }

  return 0;
}

void httpclient_shutdown(struct httpclient_response *response) {
  csocket_client_close(&response->connection);
}

void httpclient_free(struct httpclient_response *response) {
  frequenc_safe_free(response->body);
}
