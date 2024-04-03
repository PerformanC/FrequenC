/*
  CSocket-server: A simple (cross-platform) and lightweight C socket server library.

  License available on: licenses/performanc.license
*/

#ifndef CSOCKET_H
#define CSOCKET_H

#ifdef CSOCKET_SECURE
  #include <openssl/ssl.h>
  #include <openssl/err.h>
#endif

#ifdef _WIN32
  // Not supported yet
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
#endif

#include <stdio.h>

struct csocket_server {
  struct csocket_server_config *config;
  int socket;
  #ifdef _WIN32
    // Not supported yet
  #else
    struct sockaddr_in address;
  #endif
  int port;
  #ifdef CSOCKET_SECURE
    SSL_CTX *ctx;
  #endif
};

struct csocket_server_client {
  int socket;
  #ifdef _WIN32
    // Not supported yet
  #else
    struct sockaddr_in address;
  #endif
  #ifdef CSOCKET_SECURE
    SSL *ssl;
  #endif
};

int csocket_server_init(struct csocket_server *server);

int csocket_server_accept(struct csocket_server server, struct csocket_server_client *client);

int csocket_server_send(struct csocket_server_client *client, char *data, size_t length);

int csocket_close_client(struct csocket_server_client *client);

int csocket_server_recv(struct csocket_server_client *client, char *buffer, size_t length);

int csocket_server_close(struct csocket_server *server);

unsigned int csocket_server_client_get_id(struct csocket_server_client *client);

char *csocket_server_client_get_ip(struct csocket_server_client *client);

unsigned int csocket_server_client_get_port(struct csocket_server_client *client);

#endif
