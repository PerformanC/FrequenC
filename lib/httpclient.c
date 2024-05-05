#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509_vfy.h>
#include <openssl/x509v3.h>

#include "tcplimits.h"

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
  ERR_load_crypto_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();

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

  http_response->ctx = SSL_CTX_new(TLS_client_method());
  if (!http_response->ctx) {
    perror("[https-client]: Failed to create SSL context");

    SSL_CTX_free(http_response->ctx);
    close(http_response->socket);

    return -1;
  }

  if (!SSL_CTX_set_min_proto_version(http_response->ctx, TLS1_2_VERSION)) {
    perror("[https-client]: Failed to set minimum TLS version");

    SSL_CTX_free(http_response->ctx);
    close(http_response->socket);

    return -1;
  }

  http_response->ssl = SSL_new(http_response->ctx);
  if (!http_response->ssl) {
    perror("[https-client]: Failed to create SSL");

    goto exit_fail;
  }

  if (!SSL_set_fd(http_response->ssl, http_response->socket)) {
    perror("[https-client]: Failed to attach SSL to socket");

    goto exit_fail;
  }

  SSL_CTX_set_verify(http_response->ctx, SSL_VERIFY_PEER, NULL);

  if (!SSL_CTX_set_default_verify_paths(http_response->ctx)) {
    perror("[https-client]: Failed to set default verify paths");

    goto exit_fail;
  }

  if (SSL_set1_host(http_response->ssl, request->host) != 1) {
    perror("[https-client]: Failed to set hostname for certificate validation");

    goto exit_fail;
  }

  if (SSL_set_tlsext_host_name(http_response->ssl, request->host) != 1) {
    perror("[https-client]: Failed to set hostname for SNI");

    goto exit_fail;
  }

  if (SSL_get_verify_result(http_response->ssl) != X509_V_OK) {
    perror("[https-client]: Failed to verify server certificate");

    goto exit_fail;
  }

  if (SSL_connect(http_response->ssl) != 1) {
    printf("[https-client]: Failed to perform SSL handshake: %d\n", SSL_get_error(http_response->ssl, 0));

    goto exit_fail;
  }

  struct tstr_string http_request;
  _httpclient_build_request(request, &http_request);

  if (SSL_write(http_response->ssl, http_request.string, http_request.length) == -1) {
    perror("[https-client]: Failed to send HTTP request");

    goto exit_fail;
  }
  frequenc_unsafe_free(http_request.string);

  char packet[TCPLIMITS_PACKET_SIZE];

  int len = SSL_read(http_response->ssl, packet, TCPLIMITS_PACKET_SIZE);
  if (len == -1) {
    perror("[https-client]: Failed to receive HTTP response");

    goto exit_fail;
  }

  struct httpparser_header headers[30];
  httpparser_init_response((struct httpparser_response *)http_response, headers, 30);

  if (httpparser_parse_response((struct httpparser_response *)http_response, packet, len) == -1) {
    printf("[https-client]: Failed to parse HTTP response.\n");

    goto exit_fail;
  }

  if (http_response->finished == 0) {
    int chunk_size_left = http_response->chunk_length - http_response->body_length;
    
    read_chunk:

    http_response->body = realloc(http_response->body, http_response->body_length + chunk_size_left + 1);

    if (http_response->body == NULL) {
      perror("[https-client]: Failed to allocate memory for response");

      goto exit_fail;
    }

    while (chunk_size_left > 0) {
      len = SSL_read(http_response->ssl, packet, chunk_size_left > TCPLIMITS_PACKET_SIZE ? TCPLIMITS_PACKET_SIZE : chunk_size_left);
      if (len == -1) {
        perror("[https-client]: Failed to receive HTTP response");

        goto exit_fail;
      }

      tstr_append(http_response->body, packet, &http_response->body_length, len);
      chunk_size_left -= len;

      if (chunk_size_left == 0) break;
    }

    len = SSL_read(http_response->ssl, packet, 2);
    if (len == -1) {
      perror("[https-client]: Failed to receive HTTP response");

      goto exit_fail;
    }

    char chunk_size_str[5];
    int i = 0;
    while (1) {
      len = SSL_read(http_response->ssl, packet, 1);
      if (len == -1) {
        perror("[https-client]: Failed to receive HTTP response");

        goto exit_fail;
      }

      if (packet[0] == '\r') break;

      chunk_size_str[i] = packet[0];
      i++;
    }

    chunk_size_str[i] = '\0';

    chunk_size_left = strtol(chunk_size_str, NULL, 16);
    if (chunk_size_left == 0) return 0;

    /* \r has been already read */
    len = SSL_read(http_response->ssl, packet, 1);
    if (len == -1) {
      perror("[https-client]: Failed to receive HTTP response");

      goto exit_fail;
    }

    goto read_chunk;
  }

  return 0;

  exit_fail: {
    SSL_free(http_response->ssl);
    SSL_CTX_free(http_response->ctx);
    close(http_response->socket);

    return -1;
  }
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
  SSL_shutdown(response->ssl);
  SSL_free(response->ssl);
  SSL_CTX_free(response->ctx);
  close(response->socket);
}

void httpclient_free(struct httpclient_response *response) {
  frequenc_safe_free(response->body);
}
