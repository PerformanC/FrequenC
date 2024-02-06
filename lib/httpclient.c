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

  length += strlen(request->method) + 1;
  length += strlen(request->path) + 1;
  length += (sizeof("HTTP/1.1") - 1) + (sizeof("\r\nHost: \r\n") - 1) + strlen(request->host);

  for (int i = 0; i < request->headersLength; i++) {
    length += strlen(request->headers[i].key);
    length += strlen(request->headers[i].value);

    length += 4;
  }

  if (request->body != NULL) {
    length += (sizeof("Content-Length: \r\n\r\n") - 1) + strlen(request->body);
    length += snprintf(NULL, 0, "%ld", strlen(request->body));
  } else {
    length += 2;
  }

  return length;
}

void _httpclient_build_request(struct httpclient_request_params *request, struct tstr_string *response) {
  /* todo: re-use body strlen */
  size_t length = _httpclient_calculate_request_length(request);
  size_t currentLength = 0;
  /* todo: use non-NULL terminated strings (?) */
  char *requestString = frequenc_safe_malloc((length + 1) * sizeof(char));

  currentLength = snprintf(requestString, length, "%s %s HTTP/1.1\r\nHost: %s\r\n", request->method, request->path, request->host);

  for (int i = 0; i < request->headersLength; i++) {
    currentLength += snprintf(requestString + currentLength, (length + 1) - currentLength, "%s: %s\r\n", request->headers[i].key, request->headers[i].value);
  }

  if (request->body != NULL) {
    currentLength += snprintf(requestString + currentLength, (length + 1) - currentLength, "Content-Length: %ld\r\n\r\n%s", strlen(request->body), request->body);
  } else {
    currentLength += snprintf(requestString + currentLength, (length + 1) - currentLength, "\r\n");
  }

  requestString[currentLength] = '\0';

  response->string = requestString;
  response->length = length;
}

int httpclient_request(struct httpclient_request_params *request, struct httpclient_response *httpResponse) {
  ERR_load_crypto_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();

  httpResponse->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (httpResponse->socket == -1) {
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

  if (connect(httpResponse->socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("[https-client]: Failed to connect to server");

    return -1;
  }

  httpResponse->ctx = SSL_CTX_new(TLS_client_method());
  if (!httpResponse->ctx) {
    perror("[https-client]: Failed to create SSL context");

    SSL_CTX_free(httpResponse->ctx);
    close(httpResponse->socket);

    return -1;
  }

  if (!SSL_CTX_set_min_proto_version(httpResponse->ctx, TLS1_2_VERSION)) {
    perror("[https-client]: Failed to set minimum TLS version");

    SSL_CTX_free(httpResponse->ctx);
    close(httpResponse->socket);

    return -1;
  }

  httpResponse->ssl = SSL_new(httpResponse->ctx);
  if (!httpResponse->ssl) {
    perror("[https-client]: Failed to create SSL");

    goto exit_fail;
  }

  if (!SSL_set_fd(httpResponse->ssl, httpResponse->socket)) {
    perror("[https-client]: Failed to attach SSL to socket");

    goto exit_fail;
  }

  SSL_CTX_set_verify(httpResponse->ctx, SSL_VERIFY_PEER, NULL);

  if (!SSL_CTX_set_default_verify_paths(httpResponse->ctx)) {
    perror("[https-client]: Failed to set default verify paths");

    goto exit_fail;
  }

  if (SSL_set1_host(httpResponse->ssl, request->host) != 1) {
    perror("[https-client]: Failed to set hostname for certificate validation");

    goto exit_fail;
  }

  if (SSL_set_tlsext_host_name(httpResponse->ssl, request->host) != 1) {
    perror("[https-client]: Failed to set hostname for SNI");

    goto exit_fail;
  }

  if (SSL_get_verify_result(httpResponse->ssl) != X509_V_OK) {
    perror("[https-client]: Failed to verify server certificate");

    goto exit_fail;
  }

  if (SSL_connect(httpResponse->ssl) != 1) {
    printf("[https-client]: Failed to perform SSL handshake: %d\n", SSL_get_error(httpResponse->ssl, 0));

    goto exit_fail;
  }

  struct tstr_string httpRequest;
  _httpclient_build_request(request, &httpRequest);

  if (SSL_write(httpResponse->ssl, httpRequest.string, httpRequest.length) == -1) {
    perror("[https-client]: Failed to send HTTP request");

    goto exit_fail;
  }
  free(httpRequest.string);

  char packet[TCPLIMITS_PACKET_SIZE + 1];

  int len = SSL_read(httpResponse->ssl, packet, TCPLIMITS_PACKET_SIZE);
  if (len == -1) {
    perror("[https-client]: Failed to receive HTTP response");

    goto exit_fail;
  }
  packet[len] = '\0';

  struct httpparser_header headers[30];
  httpparser_init_response((struct httpparser_response *)httpResponse, headers, 30);

  if (httpparser_parse_response((struct httpparser_response *)httpResponse, packet) == -1) {
    printf("[https-client]: Failed to parse HTTP response.\n");

    goto exit_fail;
  }

  if (httpResponse->finished == 0) {
    int chunkSizeLeft = httpResponse->chunkLength - httpResponse->bodyLength;
    
    readChunk:

    httpResponse->body = realloc(httpResponse->body, httpResponse->bodyLength + chunkSizeLeft + 1);

    if (httpResponse->body == NULL) {
      perror("[https-client]: Failed to allocate memory for response");

      goto exit_fail;
    }

    while (chunkSizeLeft > 0) {
      len = SSL_read(httpResponse->ssl, packet, chunkSizeLeft > TCPLIMITS_PACKET_SIZE ? TCPLIMITS_PACKET_SIZE : chunkSizeLeft);
      if (len == -1) {
        perror("[https-client]: Failed to receive HTTP response");

        goto exit_fail;
      }

      packet[len] = '\0';

      tstr_append(httpResponse->body, packet, &httpResponse->bodyLength, len);
      chunkSizeLeft -= len;

      if (chunkSizeLeft == 0) break;
    }

    len = SSL_read(httpResponse->ssl, packet, 2);
    if (len == -1) {
      perror("[https-client]: Failed to receive HTTP response");

      goto exit_fail;
    }

    char chunkSizeStr[5];
    int i = 0;
    while (1) {
      len = SSL_read(httpResponse->ssl, packet, 1);
      if (len == -1) {
        perror("[https-client]: Failed to receive HTTP response");

        goto exit_fail;
      }

      packet[len] = '\0';

      if (packet[0] == '\r') {
        break;
      }

      chunkSizeStr[i] = packet[0];
      i++;
    }

    chunkSizeStr[i] = '\0';

    chunkSizeLeft = strtol(chunkSizeStr, NULL, 16);
    if (chunkSizeLeft == 0) return 0;

    /* \r has been already read */
    len = SSL_read(httpResponse->ssl, packet, 1);
    if (len == -1) {
      perror("[https-client]: Failed to receive HTTP response");

      goto exit_fail;
    }

    goto readChunk;
  }

  return 0;

  exit_fail: {
    SSL_free(httpResponse->ssl);
    SSL_CTX_free(httpResponse->ctx);
    close(httpResponse->socket);

    return -1;
  }
}

int httpclient_unsafe_request(struct httpclient_request_params *request, struct httpclient_response *httpResponse) {
  httpResponse->socket = socket(AF_INET, SOCK_STREAM, 0);
  if (httpResponse->socket == -1) {
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

  if (connect(httpResponse->socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("[http-client]: Failed to connect to server");

    return -1;
  }

  struct tstr_string httpRequest;
  _httpclient_build_request(request, &httpRequest);

  if (write(httpResponse->socket, httpRequest.string, httpRequest.length - 1) == -1) {
    perror("[http-client]: Failed to send HTTP request");

    return -1;
  }
  free(httpRequest.string);

  char packet[TCPLIMITS_PACKET_SIZE + 1];

  int len = read(httpResponse->socket, packet, TCPLIMITS_PACKET_SIZE);
  if (len == -1) {
    perror("[http-client]: Failed to receive HTTP response");

    return -1;
  }
  packet[len] = '\0';

  struct httpparser_header headers[30];
  httpparser_init_response((struct httpparser_response *)httpResponse, headers, 30);

  if (httpparser_parse_response((struct httpparser_response *)httpResponse, packet) == -1) {
    printf("[http-client]: Failed to parse HTTP response.\n");

    return -1;
  }

  if (httpResponse->finished == 0) {
    int chunkSizeLeft = httpResponse->chunkLength - httpResponse->bodyLength;
    
    readChunk:

    httpResponse->body = realloc(httpResponse->body, httpResponse->bodyLength + chunkSizeLeft + 1);

    if (httpResponse->body == NULL) {
      perror("[http-client]: Failed to allocate memory for response");

      return -1;
    }

    while (chunkSizeLeft > 0) {
      len = read(httpResponse->socket, packet, chunkSizeLeft > TCPLIMITS_PACKET_SIZE ? TCPLIMITS_PACKET_SIZE : chunkSizeLeft);
      if (len == -1) {
        perror("[https-client]: Failed to receive HTTP response");

        goto exit_fail;
      }

      packet[len] = '\0';

      tstr_append(httpResponse->body, packet, &httpResponse->bodyLength, len);
      chunkSizeLeft -= len;

      if (chunkSizeLeft == 0) break;
    }

    len = read(httpResponse->socket, packet, 2);
    if (len == -1) {
      perror("[http-client]: Failed to receive HTTP response");

      return -1;
    }

    char chunkSizeStr[5];
    int i = 0;
    while (1) {
      len = read(httpResponse->socket, packet, 1);
      if (len == -1) {
        perror("[http-client]: Failed to receive HTTP response");

        return -1;
      }

      packet[len] = '\0';

      if (packet[0] == '\r') {
        break;
      }

      chunkSizeStr[i] = packet[0];
      i++;
    }

    chunkSizeStr[i] = '\0';

    chunkSizeLeft = strtol(chunkSizeStr, NULL, 16);
    if (chunkSizeLeft == 0) return 0;

    /* \r has been already read */
    len = read(httpResponse->socket, packet, 1);
    if (len == -1) {
      perror("[http-client]: Failed to receive HTTP response");

      return -1;
    }

    goto readChunk;
  }

  return 0;

  exit_fail: {
    close(httpResponse->socket);

    return -1;
  }
}

void httpclient_shutdown(struct httpclient_response *response) {
  if (SSL_shutdown(response->ssl) == -1) {
    perror("[https-client]: Failed to shutdown SSL connection");
  }

  SSL_free(response->ssl);
  SSL_CTX_free(response->ctx);
  close(response->socket);
}

void httpclient_free(struct httpclient_response *response) {
  if (response->body != NULL) free(response->body);
}
