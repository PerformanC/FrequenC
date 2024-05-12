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

#include "csocket-client.h"

void _csocket_client_close(struct csocket_client *client) {
  #ifdef _WIN32
    closesocket(client->socket);
    WSACleanup();
  #else
    close(client->socket);
  #endif

  if (client->secure) {
    pcll_shutdown(&client->connection);
    pcll_free(&client->connection);
  }
}

int csocket_client_init(struct csocket_client *client, bool secure, char *hostname, int port) {
  #ifdef _WIN32
    if (WSAStartup(MAKEWORD(2,2), &client->wsa) != 0) {
      perror("[csocket-client]: Failed to initialize Winsock");

      return -1;
    }

    if ((client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
      perror("[csocket-client]: Failed to create socket");

      return -1;
    }
  #else
    if ((client->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      perror("[csocket-client]: Failed to create socket");

      return -1;
    }
  #endif

  struct hostent *host = gethostbyname(hostname);
  if (!host) {
    perror("[https-client]: Failed to resolve hostname");

    return -1;
  }

  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = htons(port),
    .sin_addr = *((struct in_addr *)host->h_addr_list[0])
  };

  if (connect(client->socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("[csocket-client]: Failed to connect to server");

    return -1;
  }

  if ((client->secure = secure)) {
    int ret = pcll_init_ssl(&client->connection);
    if (ret != PCLL_SUCCESS) {
      _csocket_client_close(client);

      perror("[https-client]: Failed to initialize SSL");

      return -1;
    }

    ret = pcll_set_fd(&client->connection, (int)client->socket);
    if (ret != PCLL_SUCCESS) {
      _csocket_client_close(client);

      perror("[https-client]: Failed to attach SSL to socket");

      return -1;
    }

    ret = pcll_set_safe_mode(&client->connection, hostname);
    if (ret != PCLL_SUCCESS) {
      _csocket_client_close(client);

      perror("[https-client]: Failed to set safe mode");

      return -1;
    }

    ret = pcll_connect(&client->connection);
    if (ret != PCLL_SUCCESS) {
      _csocket_client_close(client);

      printf("[https-client]: Failed to perform SSL handshake: %d\n", pcll_get_error(&client->connection, ret));

      return -1;
    }
  }

  return CSOCKET_CLIENT_SUCCESS;
}

int csocket_client_send(struct csocket_client *client, char *data, int size) {
  if (client->secure) {
    int ret = pcll_send(&client->connection, data, size);
    if (ret == PCLL_ERROR) {
      perror("[https-client]: Failed to send data");

      return CSOCKET_CLIENT_ERROR;
    }
  } else {
    if (send(client->socket, data, size, 0) < 0) {
      perror("[csocket-client]: Failed to send data");

      return CSOCKET_CLIENT_ERROR;
    }
  }

  return CSOCKET_CLIENT_SUCCESS;
}

int csocket_client_recv(struct csocket_client *client, char *buffer, int size) {
  if (client->secure) {
    int recv_length = pcll_recv(&client->connection, buffer, size);
    if (recv_length == PCLL_ERROR) {
      perror("[https-client]: Failed to receive data");

      return CSOCKET_CLIENT_ERROR;
    }

    return recv_length;
  } else {
    int recv_length = recv(client->socket, buffer, size, 0);
    if (recv_length < 0) {
      perror("[csocket-client]: Failed to receive data");

      return CSOCKET_CLIENT_ERROR;
    }

    return recv_length;
  }

  return CSOCKET_CLIENT_SUCCESS;
}

void csocket_client_close(struct csocket_client *client) {
  _csocket_client_close(client);
}
