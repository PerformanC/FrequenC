#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "httpparser.h"
#include "websocket.h"
#include "cthreads.h"
#include "utils.h"
#include "types.h"
#include "csocket-server.h"

#include "httpserver.h"

#ifndef PORT
#define PORT 8888
#endif

#define MAX_MESSAGE_LENGTH 2048

struct ConnectionData {
  struct csocket_server_client client;
  int socketIndex;
  struct httpserver *server;
  void (*callback)(struct csocket_server_client *client, int socketIndex, struct httpparser_request *request);
  int (*websocketCallback)(struct csocket_server_client *client, struct frequenc_ws_header *frameHeader);
  void (*disconnectCallback)(struct csocket_server_client *client, int socketIndex);
};

void startServer(struct httpserver *server) {
  server->server = (struct csocket_server) {
    .port = PORT
  };

  if (csocket_server_init(&server->server) == 1) {
    perror("[httpserver]: Failed to initialize server");

    exit(1);
  }

  server->sockets = frequenc_safe_malloc(sizeof(struct httpserver_client));
  server->availableSockets = frequenc_safe_malloc(sizeof(int));
  server->availableSockets[0] = 1;
  server->socketsCapacity = 1;
  server->socketsAmount = 0;

  printf("[httpserver]: Ready to accept connections on port %d\n", PORT);
}

void stopServer(struct httpserver *server) {
  csocket_server_close(&server->server);
  free(server->sockets);
  free(server->availableSockets);
}

void disconnectClient(struct csocket_server_client *client) {
  csocketCloseClient(client);
}

int _selectPosition(struct httpserver *server) {
  for (size_t i = 0; i < server->socketsCapacity; i++) {
    if (server->availableSockets[i] != 0) {
      int socketIndex = server->availableSockets[i];
      server->availableSockets[i] = 0;

      printf("[httpserver]: Selected socket index: %d with available sockets: %zu.\n", socketIndex, server->socketsAmount);
      return socketIndex - 1;
    }
  }

  printf("[httpserver]: No available sockets, allocating more: from %zu to %zu.\n", server->socketsCapacity, server->socketsCapacity * 2);

  server->socketsCapacity *= 2;
  server->sockets = realloc(server->sockets, server->socketsCapacity * sizeof(struct httpserver_client));
  server->availableSockets = realloc(server->availableSockets, server->socketsCapacity * sizeof(int));

  for (size_t i = server->socketsCapacity / 2; i < server->socketsCapacity; i++) {
    server->sockets[i].socket = 0;
    server->sockets[i].upgraded = false;
    server->sockets[i].data = NULL;

    server->availableSockets[i] = i + 1;
  }

  server->availableSockets[server->socketsAmount] = 0;

  printf("[httpserver]: (REALLOC) Selected socket index: %zu with available sockets: %zu.\n", server->socketsAmount, server->socketsAmount);
  return server->socketsAmount;
}

void _addAvailableSocket(struct httpserver *server, int socketIndex) {
  for (size_t i = 0; i < server->socketsCapacity; i++) {
    if (server->availableSockets[i] == 0) {
      server->availableSockets[i] = socketIndex + 1;

      break;
    }
  }
}

void *listenPayload(void *args) {
  struct ConnectionData *connectionData = (struct ConnectionData *)args;

  struct csocket_server_client client = connectionData->client;
  int socketIndex = connectionData->socketIndex;
  struct httpserver *server = connectionData->server;

  char payload[MAX_MESSAGE_LENGTH];
  int payloadSize = 0;

  struct httpparser_request request;

  while ((payloadSize = csocket_server_recv(&client, payload, MAX_MESSAGE_LENGTH)) > 0) {
    printf("[httpserver]: Received payload.\n - Socket: %d\n - Socket index: %d\n - Payload size: %d\n", csocket_server_client_get_id(&client), socketIndex, payloadSize);

    if (server->sockets[socketIndex].upgraded) {
      struct frequenc_ws_header frameHeader = frequenc_parse_ws_header(payload);

      if (connectionData->websocketCallback(&client, &frameHeader)) goto disconnect;

      continue;
    }

    struct httpparser_header headers[10];
    httpparser_init_request(&request, headers, 10);

    if (httpparser_parse_request(&request, payload) != 0) {
      printf("[httpparser]: Failed to parse request.\n - Socket: %d\n", csocket_server_client_get_id(&client));

      goto invalid_request;
    }

    connectionData->callback(&client, socketIndex, &request);

    if (!server->sockets[socketIndex].upgraded) break;
  }

  if (payloadSize == -1)
    perror("[httpserver]: recv failed");

  disconnect: {
    connectionData->disconnectCallback(&client, socketIndex);

    disconnectClient(&client);

    server->sockets[socketIndex].socket = 0;
    server->sockets[socketIndex].upgraded = false;

    httpparser_free_request(&request);
    free(connectionData);
    cthreads_thread_detach(cthreads_thread_self());

    server->socketsAmount--;

    _addAvailableSocket(server, socketIndex);

    printf("[main]: Client disconnected.\n - Socket: %d\n - Socket index: %d\n - Thread ID: %lu\n", csocket_server_client_get_id(&client), socketIndex, cthreads_thread_id(cthreads_thread_self()));

    return NULL;
  }

  invalid_request: {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[2];
    initResponse(&response, headerBuffer, 2);

    setResponseSocket(&response, &client);
    setResponseStatus(&response, 400);

    sendResponse(&response);

    goto disconnect;
  }
}

void handleRequest(struct httpserver *server, void (*callback)(struct csocket_server_client *client, int socketIndex, struct httpparser_request *request), int (*websocketCallback)(struct csocket_server_client *client, struct frequenc_ws_header *frameHeader), void (*disconnectCallback)(struct csocket_server_client *client, int socketIndex)) {
  int socket = 0;

  struct csocket_server_client client = { 0 };
  while ((socket = csocket_server_accept(server->server, &client)) == 0) {
    int socketIndex = _selectPosition(server);

    printf("[httpserver]: Connection accepted.\n - Socket: %d\n - Socket index: %d\n - IP: %s\n - Port: %d\n", socket, socketIndex, csocket_server_client_get_ip(&client), csocket_server_client_get_port(&client));

    struct httpserver_client httpClient = {
      .socket = socket,
      .upgraded = false,
      .data = NULL
    };

    server->sockets[socketIndex] = httpClient;

    struct ConnectionData *args = frequenc_safe_malloc(sizeof(struct ConnectionData));

    args->server = server;
    args->client = client;
    args->socketIndex = server->socketsAmount;
    args->callback = callback;
    args->websocketCallback = websocketCallback;
    args->disconnectCallback = disconnectCallback;

    struct cthreads_thread thread;
    struct cthreads_args cargs;
    cthreads_thread_create(&thread, NULL, listenPayload, args, &cargs);

    server->socketsAmount++;
  }

  printf("[httpserver]: Accept failed: %d\n", socket);

  perror("[httpserver]: Accept failed");
}

void setSocketData(struct httpserver *server, int socketIndex, void *data) {
  server->sockets[socketIndex].data = data;
}

void *getSocketData(struct httpserver *server, int socketIndex) {
  return server->sockets[socketIndex].data;
}

void initResponse(struct httpserver_response *response, struct httpserver_header *headers, int length) {
  response->client = NULL;
  response->status = 200;
  response->headersLength = 0;
  response->headersMaxLength = length;
  response->headers = headers;
  response->body = NULL;
}

void setResponseSocket(struct httpserver_response *response, struct csocket_server_client *client) {
  response->client = client;
}

void setResponseStatus(struct httpserver_response *response, int status) {
  response->status = status;
}

void setResponseHeader(struct httpserver_response *response, char *key, char *value) {
  if (response->headersLength == response->headersMaxLength) {
    printf("[httpserver]: Headers length is greater than headers max length.\n");

    return;
  }

  response->headers[response->headersLength].key = key;
  response->headers[response->headersLength].value = value;

  response->headersLength++;
}

void setResponseBody(struct httpserver_response *response, char *body) {
  response->body = body;
}

char *_getStatusText(int status) {
  switch (status) {
    case 100: return "Continue";
    case 101: return "Switching Protocols";
    case 102: return "Processing";
    case 103: return "Early Hints";
    case 200: return "OK";
    case 201: return "Created";
    case 202: return "Accepted";
    case 203: return "Non-Authoratative Information";
    case 204: return "No Content";
    case 205: return "Reset Content";
    case 206: return "Partial Content";
    case 207: return "Multi-Status";
    case 208: return "Already Reported";
    case 226: return "IM Used";
    case 300: return "Multiple Choices";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 303: return "See Other";
    case 304: return "Not Modified";
    case 305: return "Use Proxy";
    case 307: return "Temporary Redirect";
    case 308: return "Permanent Redirect";
    case 400: return "Bad request";
    case 401: return "Unathorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 406: return "Not Acceptable";
    case 407: return "Proxy Authentication Required";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 411: return "Length Required";
    case 412: return "Precondition Failed";
    case 413: return "Payload Too Large";
    case 414: return "Request-URI Too Long";
    case 415: return "Unsupported Media Type";
    case 416: return "Requested Range Not Satisfiable";
    case 417: return "Expectation Failed";
    case 418: return "I'm a teapot";
    case 421: return "Misdirected Request";
    case 422: return "Unprocessable Entity";
    case 423: return "Locked";
    case 424: return "Failed Dependency";
    case 425: return "Too Early";
    case 426: return "Upgrade Required";
    case 428: return "Precondition Required";
    case 429: return "Too Many Requests";
    case 431: return "Request Header Fields Too Large";
    case 450: return "Blocked by Windows Parental Controls";
    case 451: return "Unavailable For Legal Reasons";
    case 497: return "HTTP Request Sent to HTTPS Port";
    case 498: return "Invalid Token";
    case 499: return "Client Closed Request";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    case 506: return "Variant Also Negotiates";
    case 507: return "Insufficient Storage";
    case 508: return "Loop Detected";
    case 509: return "Bandwidth Limit Exceeded";
    case 510: return "Not Extended";
    case 511: return "Network Authentication Required";
    case 521: return "Web Server Is Down";
    case 522: return "Connection Timed Out";
    case 523: return "Origin Is Unreachable";
    case 524: return "A Timeout Occurred";
    case 525: return "SSL Handshake Failed";
    case 530: return "Site Frozen";
    case 599: return "Network Connect Timeout Error";
  }

  return "Unknown";
}

size_t calculateResponseLength(struct httpserver_response *response) {
  size_t length = 0;

  char header[1024];
  length += snprintf(header, sizeof(header), "HTTP/1.1 %d %s\r\n", response->status, _getStatusText(response->status));

  for (int i = 0; i < response->headersLength; i++) {
    length += strlen(response->headers[i].key);
    length += strlen(response->headers[i].value);
    length += 4;
  }

  length += 2;

  if (response->body != NULL) {
    length += strlen(response->body);
  }

  return length;
}

void sendResponse(struct httpserver_response *response) {
  setResponseHeader(response, "Connection", "close");

  size_t length = calculateResponseLength(response);

  char *responseString = frequenc_safe_malloc(length + 1);

  snprintf(responseString, length, "HTTP/1.1 %d %s\r\n", response->status, _getStatusText(response->status));

  for (int i = 0; i < response->headersLength; i++) {
    strcat(responseString, response->headers[i].key);

    strcat(responseString, ": ");

    strcat(responseString, response->headers[i].value);

    strcat(responseString, "\r\n");
  }

  strcat(responseString, "\r\n");

  if (response->body != NULL) {
    strcat(responseString, response->body);
  }

  csocket_server_send(response->client, responseString, length);

  free(responseString);
}

void upgradeSocket(struct httpserver *server, int socketIndex) {
  server->sockets[socketIndex].upgraded = true;
}
