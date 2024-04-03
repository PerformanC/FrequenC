/*
  CSocket-server: A simple (cross-platform) and lightweight C socket server library.

  License available on: licenses/performanc.license
*/

#ifdef _WIN32
  #error "csocket-server is not supported on Windows yet"
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <signal.h>
  #include <string.h>

  #ifdef CSOCKET_SECURE
    #include <openssl/ssl.h>
    #include <openssl/err.h>
  #endif
#endif

#include "csocket-server.h"

int csocket_server_init(struct csocket_server *server) {
  #ifdef _WIN32
    // Not supported yet
  #else
    #ifdef CSOCKET_SECURE
      SSL_load_error_strings();
      OpenSSL_add_ssl_algorithms();

      const SSL_METHOD *method = TLS_server_method();
      server->ctx = SSL_CTX_new(method);

      if (!server->ctx) {
        perror("[csocket-server]: Failed to create SSL context");

        return -1;
      }

      if (SSL_CTX_use_certificate_file(server->ctx, CSOCKET_CERT, SSL_FILETYPE_PEM) <= 0) {
        perror("[csocket-server]: Failed to load certificate");

        return -1;
      }

      if (SSL_CTX_use_PrivateKey_file(server->ctx, CSOCKET_KEY, SSL_FILETYPE_PEM) <= 0) {
        perror("[csocket-server]: Failed to load private key");

        return -1;
      }

      if (!SSL_CTX_check_private_key(server->ctx)) {
        perror("[csocket-server]: Private key does not match the public certificate");

        return -1;
      }

      server->socket = socket(AF_INET, SOCK_STREAM, 0);
      if (server->socket == -1) {
        perror("[csocket-server]: Failed to create socket");

        return -1;
      }

      struct sockaddr_in serverOptions = {
        .sin_addr.s_addr = INADDR_ANY,
        .sin_family = AF_INET,
        .sin_port = htons(server->port)
      };

      if (bind(server->socket, (struct sockaddr *)&serverOptions, sizeof(serverOptions)) < 0) {
        perror("[csocket-server]: Failed to bind socket");

        return -1;
      }

      if (listen(server->socket, 3) < 0) {
        perror("[csocket-server]: Failed to listen on socket");

        return -1;
      }

      setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &serverOptions, sizeof(serverOptions));
    #else
      if ((server->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[csocket-server]: Failed to create socket");

        return -1;
      }

      struct sockaddr_in serverOptions = {
        .sin_addr.s_addr = INADDR_ANY,
        .sin_family = AF_INET,
        .sin_port = htons(server->port)
      };

      setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &serverOptions, sizeof(serverOptions));

      if (bind(server->socket, (struct sockaddr *)&serverOptions, sizeof(serverOptions)) < 0) {
        perror("[csocket-server]: Failed to bind socket");

        return -1;
      }

      if (listen(server->socket, -1) < 0) {
        perror("[csocket-server]: Failed to listen on socket");

        return -1;
      }
    #endif
  #endif

  return 0;
}

int csocket_server_accept(struct csocket_server server, struct csocket_server_client *client) {
  #ifdef _WIN32
    // Not supported yet
  #else
    socklen_t addrlen = sizeof(client->address);

    if ((client->socket = accept(server.socket, (struct sockaddr *)&client->address, &addrlen)) < 0) {
      perror("[csocket-server]: Failed to accept connection");

      return -1;
    }

    #ifdef CSOCKET_SECURE
      client->ssl = SSL_new(server->ctx);
      SSL_set_fd(client->ssl, client->socket);

      if (SSL_accept(client->ssl) <= 0) {
        perror("[csocket-server]: Failed to accept SSL connection");

        return -1;
      }
    #endif
  #endif

  return 0;
}

int csocket_server_send(struct csocket_server_client *client, char *data, size_t length) {
  #ifdef _WIN32
    // Not supported yet
  #else
    #ifdef CSOCKET_SECURE
      if (SSL_write(client->ssl, data, length) <= 0) {
        perror("[csocket-server]: Failed to send data");

        return -1;
      }
    #else
      if (send(client->socket, data, length, 0) < 0) {
        perror("[csocket-server]: Failed to send data");

        return -1;
      }
    #endif
  #endif

  return 0;
}

int csocket_close_client(struct csocket_server_client *client) {
  #ifdef _WIN32
    // Not supported yet
  #else
    #ifdef CSOCKET_SECURE
      SSL_shutdown(client->ssl);
    #endif

    close(client->socket);
  #endif

  return 0;
}

int csocket_server_recv(struct csocket_server_client *client, char *buffer, size_t length) {
  #ifdef _WIN32
    // Not supported yet
  #else
    #ifdef CSOCKET_SECURE
      int bytes = SSL_read(client->ssl, buffer, length);
      if (bytes <= 0) {
        perror("[csocket-server]: Failed to receive data");

        return -1;
      }

      buffer[bytes] = '\0';

      return bytes;
    #else
      int bytes = recv(client->socket, buffer, length, 0);
      if (bytes < 0) {
        perror("[csocket-server]: Failed to receive data");

        return -1;
      }

      buffer[bytes] = '\0';

      return bytes;
    #endif
  #endif
}

int csocket_server_close(struct csocket_server *server) {
  #ifdef _WIN32
    // Not supported yet
  #else
    #ifdef CSOCKET_SECURE
      SSL_CTX_free(server->ctx);
    #endif

    close(server->socket);
  #endif

  return 0;
}

unsigned int csocket_server_client_get_id(struct csocket_server_client *client) {
  #ifdef _WIN32
    // Not supported yet
  #else
    return client->socket;
  #endif
}

char *csocket_server_client_get_ip(struct csocket_server_client *client) {
  #ifdef _WIN32
    // Not supported yet
  #else
    return inet_ntoa(client->address.sin_addr);
  #endif
}

unsigned int csocket_server_client_get_port(struct csocket_server_client *client) {
  #ifdef _WIN32
    // Not supported yet
  #else
    return ntohs(client->address.sin_port);
  #endif
}
