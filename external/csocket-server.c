/*
  CSocket-server: A simple (cross-platform) and lightweight C socket server library.

  License available on: licenses/performanc.license
*/

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <unistd.h>

  #ifdef CSOCKET_SECURE
    #include "pcll.h"
  #endif
#endif

#include "csocket-server.h"

int csocket_server_init(struct csocket_server *server) {
  #ifdef _WIN32
    if (WSAStartup(MAKEWORD(2,2), &server->wsa) != 0) {
      perror("[csocket-server]: Failed to initialize Winsock");

      return -1;
    }
  #endif

  #ifdef CSOCKET_SECURE
    pcll_init_ssl_library();

    if (pcll_init_tls_server(&server->connection, CSOCKET_CERT, CSOCKET_KEY) < 0) {
      perror("[csocket-server]: Failed to initialize SSL server");

      return -1;
    }
  #endif

  #ifdef _WIN32
    server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server->socket == INVALID_SOCKET) {
      perror("[csocket-server]: Failed to create socket");

      return -1;
    }
  #else
    server->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->socket < 0) {
      perror("[csocket-server]: Failed to create socket");

      return -1;
    }
  #endif

  struct sockaddr_in server_options = {
    .sin_addr.s_addr = INADDR_ANY,
    .sin_family = AF_INET,
    .sin_port = htons(server->port)
  };

  #ifdef _WIN32
    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&server_options, sizeof(server_options)) == SOCKET_ERROR) {
      perror("[csocket-server]: Failed to set socket options");

      return -1;
    }
  #else
    if (setsockopt(server->socket, SOL_SOCKET, SO_REUSEADDR, &server_options, sizeof(server_options)) < 0) {
      perror("[csocket-server]: Failed to set socket options");

      return -1;
    }
  #endif

  if (bind(server->socket, (struct sockaddr *)&server_options, sizeof(server_options)) < 0) {
    perror("[csocket-server]: Failed to bind socket");

    return -1;
  }

  if (listen(server->socket, -1) < 0) {
    perror("[csocket-server]: Failed to listen on socket");

    return -1;
  }

  return 0;
}

int csocket_server_accept(struct csocket_server server, struct csocket_server_client *client) {
  socklen_t addrlen = sizeof(client->address);

  if ((client->socket = accept(server.socket, (struct sockaddr *)&client->address, &addrlen)) < 0) {
    perror("[csocket-server]: Failed to accept connection");

    return -1;
  }

  #ifdef CSOCKET_SECURE
    pcll_init_only_ssl(&client->connection);
    pcll_set_fd(&client->connection, client->socket);

    if (pcll_accept(&client->connection) < 0) {
      perror("[csocket-server]: Failed to accept SSL connection");

      return -1;
    }
  #endif

  return 0;
}

int csocket_server_send(struct csocket_server_client *client, char *data, size_t length) {
  #ifdef CSOCKET_SECURE
    if (pcll_send(client->connection, data, length) < 0) {
      perror("[csocket-server]: Failed to send data");

      return -1;
    }
  #else
    if (send(client->socket, data, length, 0) < 0) {
      perror("[csocket-server]: Failed to send data");

      return -1;
    }
  #endif

  return 0;
}

int csocket_close_client(struct csocket_server_client *client) {
  #ifdef CSOCKET_SECURE
    pcll_shutdown(client->connection);
  #endif

  #ifdef _WIN32
    closesocket(client->socket);
  #else
    close(client->socket);
  #endif

  return 0;
}

long csocket_server_recv(struct csocket_server_client *client, char *buffer, size_t length) {
  #ifdef CSOCKET_SECURE
    int bytes = pcll_recv(client->connection, buffer, length);
    if (bytes <= 0) {
      perror("[csocket-server]: Failed to receive data");

      return -1;
    }

    return bytes;
  #else
    long bytes = recv(client->socket, buffer, length, 0);
    if (bytes < 0) {
      perror("[csocket-server]: Failed to receive data");

      return -1;
    }

    return bytes;
  #endif
}

int csocket_server_close(struct csocket_server *server) {
  #ifdef CSOCKET_SECURE
    pcll_free(&server->connection);
  #endif

  #ifdef _WIN32
    closesocket(server->socket);
    WSACleanup();
  #else
    close(server->socket);
  #endif

  return 0;
}

unsigned int csocket_server_client_get_id(struct csocket_server_client *client) {
  return (unsigned int)client->socket;
}

const char *csocket_server_client_get_ip(struct csocket_server_client *client, char *ip) {
  return inet_ntop(AF_INET, &client->address.sin_addr, ip, INET_ADDRSTRLEN);
}

unsigned int csocket_server_client_get_port(struct csocket_server_client *client) {
  return ntohs(client->address.sin_port);
}
