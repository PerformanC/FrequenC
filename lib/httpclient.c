#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "tcplimits.h"
#include "pcll.h"

#include "httpparser.h"
#include "libtstr.h"
#include "utils.h"

#include "httpclient.h"

size_t _httpclient_calculate_request_length(struct httpclient_request_params *request) {
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
    length += (sizeof("Content-Length: \r\n\r\n") - 1) + snprintf(NULL, 0, "%zu", request->body->length) + request->body->length;
  } else {
    length += 2;
  }

  return length;
}

void _httpclient_build_request(struct httpclient_request_params *request, struct tstr_string *response) {
  size_t length = _httpclient_calculate_request_length(request);
  size_t current_length = 0;
  char *request_string = frequenc_safe_malloc(length * sizeof(char));

  current_length = snprintf(request_string, length, "%s %s HTTP/1.1\r\nHost: %s\r\n", request->method, request->path, request->host);

  for (int i = 0; i < request->headers_length; i++) {
    current_length += snprintf(request_string + current_length, (length + 1) - current_length, "%s: %s\r\n", request->headers[i].key, request->headers[i].value);
  }

  if (request->body != NULL) {
    current_length += snprintf(request_string + current_length, (length + 1) - current_length, "Content-Length: %zu\r\n\r\n%.*s", request->body->length, (int)request->body->length, request->body->string);
  } else {
    current_length += snprintf(request_string + current_length, (length + 1) - current_length, "\r\n");
  }

  response->string = request_string;
  response->length = length;
}

int httpclient_request(struct httpclient_request_params *request, struct httpclient_response *http_response) {
  pcll_init_ssl_library();

  http_response->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (http_response->socket == -1) {
    perror("[https-client]: Failed to create socket");

    return -1;
  }

  struct hostent *host = gethostbyname(request->host);
  if (!host) {
    perror("[https-client]: Failed to resolve hostname");

    return -1;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(request->port),
    .sin_addr = *((struct in_addr *)host->h_addr_list[0])
  };

  if (connect(http_response->socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("[https-client]: Failed to connect to server");

    return -1;
  }

  if (pcll_init_ssl(&http_response->connection) == -1) {
    close(http_response->socket);

    perror("[https-client]: Failed to initialize SSL");

    return -1;
  }

  if (pcll_set_fd(&http_response->connection, http_response->socket) == -1) {
    close(http_response->socket);

    perror("[https-client]: Failed to attach SSL to socket");

    return -1;
  }

  if (pcll_set_safe_mode(&http_response->connection, request->host) == -1) {
    close(http_response->socket);

    perror("[https-client]: Failed to set safe mode");

    return -1;
  }

  if (pcll_connect(&http_response->connection) == -1) {
    close(http_response->socket);

    printf("[https-client]: Failed to perform SSL handshake: %d\n", pcll_get_error(&http_response->connection));

    return -1;
  }

  struct tstr_string http_request;
  _httpclient_build_request(request, &http_request);

  if (pcll_send(&http_response->connection, http_request.string, http_request.length) == -1) {
    close(http_response->socket);

    perror("[https-client]: Failed to send HTTP request");

    return -1;
  }
  frequenc_unsafe_free(http_request.string);

  char packet[TCPLIMITS_PACKET_SIZE];

  int len = pcll_recv(&http_response->connection, packet, TCPLIMITS_PACKET_SIZE);
  if (len == -1) {
    close(http_response->socket);

    perror("[https-client]: Failed to receive HTTP response");

    return -1;
  }

  struct httpparser_header headers[30];
  httpparser_init_response((struct httpparser_response *)http_response, headers, 30);

  if (httpparser_parse_response((struct httpparser_response *)http_response, packet, len) == -1) {
    close(http_response->socket);
    pcll_free(&http_response->connection);

    printf("[https-client]: Failed to parse HTTP response.\n");

    return -1;
  }

  if (http_response->finished == 0) {
    int chunk_size_left = http_response->chunk_length - http_response->body_length;
    
    read_chunk:

    http_response->body = realloc(http_response->body, http_response->body_length + chunk_size_left + 1);

    if (http_response->body == NULL) {
      close(http_response->socket);

      perror("[https-client]: Failed to allocate memory for response");

      return -1;
    }

    while (chunk_size_left > 0) {
      len = pcll_recv(&http_response->connection, packet, chunk_size_left > TCPLIMITS_PACKET_SIZE ? TCPLIMITS_PACKET_SIZE : chunk_size_left);
      if (len == -1) {
        close(http_response->socket);

        perror("[https-client]: Failed to receive HTTP response");

        return -1;
      }

      tstr_append(http_response->body, packet, &http_response->body_length, len);
      chunk_size_left -= len;

      if (chunk_size_left == 0) break;
    }

    len = pcll_recv(&http_response->connection, packet, 2);
    if (len == -1) {
      close(http_response->socket);

      perror("[https-client]: Failed to receive HTTP response");

      return -1;
    }

    char chunk_size_str[5];
    int i = 0;
    while (1) {
      len = pcll_recv(&http_response->connection, packet, 1);
      if (len == -1) {
        close(http_response->socket);

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

    len = pcll_recv(&http_response->connection, packet, 1);
    if (len == -1) {
      close(http_response->socket);

      perror("[https-client]: Failed to receive HTTP response");

      return -1;
    }

    goto read_chunk;
  }

  return 0;
}

int httpclient_unsafe_request(struct httpclient_request_params *request, struct httpclient_response *http_response) {
  http_response->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (http_response->socket == -1) {
    perror("[http-client]: Failed to create socket");

    return -1;
  }

  struct hostent *host = gethostbyname(request->host);
  if (!host) {
    perror("[http-client]: Failed to resolve hostname");

    return -1;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(request->port),
    .sin_addr = *((struct in_addr *)host->h_addr_list[0])
  };

  if (connect(http_response->socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("[http-client]: Failed to connect to server");

    return -1;
  }

  struct tstr_string http_request;
  _httpclient_build_request(request, &http_request);

  if (write(http_response->socket, http_request.string, http_request.length - 1) == -1) {
    perror("[http-client]: Failed to send HTTP request");

    return -1;
  }
  frequenc_unsafe_free(http_request.string);

  char packet[TCPLIMITS_PACKET_SIZE + 1];

  int len = read(http_response->socket, packet, TCPLIMITS_PACKET_SIZE);
  if (len == -1) {
    perror("[http-client]: Failed to receive HTTP response");

    return -1;
  }

  struct httpparser_header headers[30];
  httpparser_init_response((struct httpparser_response *)http_response, headers, 30);

  if (httpparser_parse_response((struct httpparser_response *)http_response, packet, len) == -1) {
    printf("[http-client]: Failed to parse HTTP response.\n");

    return -1;
  }

  if (http_response->finished == 0) {
    int chunk_size_left = http_response->chunk_length - http_response->body_length;
    
    read_chunk:

    http_response->body = realloc(http_response->body, http_response->body_length + chunk_size_left + 1);

    if (http_response->body == NULL) {
      perror("[http-client]: Failed to allocate memory for response");

      return -1;
    }

    while (chunk_size_left > 0) {
      len = read(http_response->socket, packet, chunk_size_left > TCPLIMITS_PACKET_SIZE ? TCPLIMITS_PACKET_SIZE : chunk_size_left);
      if (len == -1) {
        perror("[https-client]: Failed to receive HTTP response");

        goto exit_fail;
      }

      tstr_append(http_response->body, packet, &http_response->body_length, len);
      chunk_size_left -= len;

      if (chunk_size_left == 0) break;
    }

    len = read(http_response->socket, packet, 2);
    if (len == -1) {
      perror("[http-client]: Failed to receive HTTP response");

      return -1;
    }

    char chunk_size_str[5];
    int i = 0;
    while (1) {
      len = read(http_response->socket, packet, 1);
      if (len == -1) {
        perror("[http-client]: Failed to receive HTTP response");

        return -1;
      }

      if (packet[0] == '\r') break;

      chunk_size_str[i] = packet[0];
      i++;
    }

    chunk_size_str[i] = '\0';

    chunk_size_left = strtol(chunk_size_str, NULL, 16);
    if (chunk_size_left == 0) return 0;

    /* \r has been already read */
    len = read(http_response->socket, packet, 1);
    if (len == -1) {
      perror("[http-client]: Failed to receive HTTP response");

      return -1;
    }

    goto read_chunk;
  }

  return 0;

  exit_fail: {
    close(http_response->socket);

    return -1;
  }
}

void httpclient_shutdown(struct httpclient_response *response) {
  pcll_shutdown(&response->connection);
  pcll_free(&response->connection);

  close(response->socket);
}

void httpclient_free(struct httpclient_response *response) {
  frequenc_safe_free(response->body);
}
