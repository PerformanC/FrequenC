#ifndef CSOCKET_CLIENT_H
#define CSOCKET_CLIENT_H

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <arpa/inet.h>
#endif

#include "pcll.h"
#include "types.h"

#define CSOCKET_CLIENT_SUCCESS 0
#define CSOCKET_CLIENT_ERROR  -1

struct csocket_client {
  #ifdef _WIN32
    SOCKET socket;
    WSADATA wsa;
  #else
    int socket;
  #endif
  struct pcll_connection connection;
  bool secure;
};

int csocket_client_init(struct csocket_client *client, bool secure, char *hostname, unsigned short port);

int csocket_client_send(struct csocket_client *client, char *data, size_t size);

long csocket_client_recv(struct csocket_client *client, char *buffer, size_t size);

void csocket_client_close(struct csocket_client *client);

#endif /* CSOCKET_CLIENT_H */
