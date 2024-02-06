#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "websocket.h"
#include "httpparser.h"
#include "csocket-server.h"
#include "types.h"

struct httpserver {
  int *availableSockets;
  struct csocket_server server;
  size_t socketsCapacity;
  size_t socketsAmount;
  struct httpserver_client *sockets;
  void (*handler)(int, char*);
};
struct httpserver_header {
  char *key;
  char *value;
};

struct httpserver_response {
  struct csocket_server_client *client;
  int status;
  int headersLength;
  int headersMaxLength;
  struct httpserver_header *headers;
  char *body;
};

struct httpserver_client {
  int socket;
  bool upgraded;
  void *data;
};

struct callbackData {
  struct httpserver *server;
  struct csocket_server_client *client;
  struct httpparser_request *request;
};

void startServer(struct httpserver *server);

void stopServer(struct httpserver *server);

void disconnectClient(struct csocket_server_client *client);

void handleRequest(struct httpserver *server, void (*callback)(struct csocket_server_client *client, int socketIndex, struct httpparser_request *request), int (*websocketCallback)(struct csocket_server_client *client, struct frequenc_ws_header *frameHeader), void (*disconnectCallback)(struct csocket_server_client *client, int socketIndex));

void setSocketData(struct httpserver *server, int socketIndex, void *data);

void *getSocketData(struct httpserver *server, int socketIndex);

void initResponse(struct httpserver_response *response, struct httpserver_header *headers, int length);

void setResponseSocket(struct httpserver_response *response, struct csocket_server_client *client);

void setResponseStatus(struct httpserver_response *response, int status);

void setResponseHeader(struct httpserver_response *response, char *key, char *value);

void setResponseBody(struct httpserver_response *response, char *body);

void sendResponse(struct httpserver_response *response);

void setSocketData(struct httpserver *server, int socket, void *data);

void upgradeSocket(struct httpserver *server, int socketIndex);

#endif /* HTTPSERVER_H_ */
