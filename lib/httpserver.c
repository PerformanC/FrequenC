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

struct _httpserver_connection_data {
  struct csocket_server_client client;
  int socket_index;
  struct httpserver *server;
  void (*callback)(struct csocket_server_client *client, int socket_index, struct httpparser_request *request);
  int (*websocket_callback)(struct csocket_server_client *client, struct frequenc_ws_frame *frame_header);
  void (*disconnect_callback)(struct csocket_server_client *client, int socket_index);
};

void httpserver_start_server(struct httpserver *server) {
  server->server = (struct csocket_server) {
    .port = PORT
  };

  if (csocket_server_init(&server->server) == 1) {
    perror("[httpserver]: Failed to initialize server");

    exit(1);
  }

  server->sockets = frequenc_safe_malloc(sizeof(struct httpserver_client));
  server->available_sockets = frequenc_safe_malloc(sizeof(int));
  server->available_sockets[0] = 1;
  server->sockets_capacity = 1;
  server->sockets_length = 0;

  printf("[httpserver]: Ready to accept connections on port %d\n", PORT);
}

void httpserver_stop_server(struct httpserver *server) {
  csocket_server_close(&server->server);
  free(server->sockets);
  free(server->available_sockets);
}

void httpserver_disconnect_client(struct csocket_server_client *client) {
  csocketCloseClient(client);
}

int _httpserver_select_position(struct httpserver *server) {
  for (size_t i = 0; i < server->sockets_capacity; i++) {
    if (server->available_sockets[i] != 0) {
      int socket_index = server->available_sockets[i];
      server->available_sockets[i] = 0;

      printf("[httpserver]: Selected socket index: %d with available sockets: %zu.\n", socket_index, server->sockets_length);
      return socket_index - 1;
    }
  }

  printf("[httpserver]: No available sockets, allocating more: from %zu to %zu.\n", server->sockets_capacity, server->sockets_capacity * 2);

  server->sockets_capacity *= 2;
  server->sockets = realloc(server->sockets, server->sockets_capacity * sizeof(struct httpserver_client));
  server->available_sockets = realloc(server->available_sockets, server->sockets_capacity * sizeof(int));

  for (size_t i = server->sockets_capacity / 2; i < server->sockets_capacity; i++) {
    server->sockets[i].socket = 0;
    server->sockets[i].upgraded = false;
    server->sockets[i].data = NULL;

    server->available_sockets[i] = i + 1;
  }

  server->available_sockets[server->sockets_length] = 0;

  printf("[httpserver]: (REALLOC) Selected socket index: %zu with available sockets: %zu.\n", server->sockets_length, server->sockets_length);

  return server->sockets_length;
}

void _httpserver_add_available_socket(struct httpserver *server, int socket_index) {
  for (size_t i = 0; i < server->sockets_capacity; i++) {
    if (server->available_sockets[i] == 0) {
      server->available_sockets[i] = socket_index + 1;

      break;
    }
  }
}

void *listen_messages(void *args) {
  struct _httpserver_connection_data *connectionData = (struct _httpserver_connection_data *)args;

  struct csocket_server_client client = connectionData->client;
  int socket_index = connectionData->socket_index;
  struct httpserver *server = connectionData->server;

  char payload[MAX_MESSAGE_LENGTH];
  int payload_size = 0;

  struct httpparser_request request;

  while ((payload_size = csocket_server_recv(&client, payload, MAX_MESSAGE_LENGTH)) > 0) {
    printf("[httpserver]: Received payload.\n - Socket: %d\n - Socket index: %d\n - Payload size: %d\n", csocket_server_client_get_id(&client), socket_index, payload_size);

    if (server->sockets[socket_index].upgraded) {
      struct frequenc_ws_frame ws_frame = frequenc_parse_ws_frame(payload);

      if (connectionData->websocket_callback(&client, &ws_frame)) goto disconnect;

      continue;
    }

    struct httpparser_header headers[10];
    httpparser_init_request(&request, headers, 10);

    if (httpparser_parse_request(&request, payload) != 0) {
      printf("[httpparser]: Failed to parse request.\n - Socket: %d\n", csocket_server_client_get_id(&client));

      goto invalid_request;
    }

    connectionData->callback(&client, socket_index, &request);

    if (!server->sockets[socket_index].upgraded) break;
  }

  if (payload_size == -1)
    perror("[httpserver]: recv failed");

  disconnect: {
    connectionData->disconnect_callback(&client, socket_index);

    httpserver_disconnect_client(&client);

    server->sockets[socket_index].socket = 0;
    server->sockets[socket_index].upgraded = false;

    httpparser_free_request(&request);
    free(connectionData);
    cthreads_thread_detach(cthreads_thread_self());

    server->sockets_length--;

    _httpserver_add_available_socket(server, socket_index);

    printf("[main]: Client disconnected.\n - Socket: %d\n - Socket index: %d\n - Thread ID: %lu\n", csocket_server_client_get_id(&client), socket_index, cthreads_thread_id(cthreads_thread_self()));

    return NULL;
  }

  invalid_request: {
    struct httpserver_response response;
    struct httpserver_header headerBuffer[2];
    httpserver_init_response(&response, headerBuffer, 2);

    httpserver_set_response_socket(&response, &client);
    httpserver_set_response_status(&response, 400);

    httpserver_send_response(&response);

    goto disconnect;
  }
}

void httpserver_handle_request(struct httpserver *server, void (*callback)(struct csocket_server_client *client, int socket_index, struct httpparser_request *request), int (*websocket_callback)(struct csocket_server_client *client, struct frequenc_ws_frame *frame_header), void (*disconnect_callback)(struct csocket_server_client *client, int socket_index)) {
  int socket = 0;

  struct csocket_server_client client = { 0 };
  while ((socket = csocket_server_accept(server->server, &client)) == 0) {
    int socket_index = _httpserver_select_position(server);

    printf("[httpserver]: Connection accepted.\n - Socket: %d\n - Socket index: %d\n - IP: %s\n - Port: %d\n", socket, socket_index, csocket_server_client_get_ip(&client), csocket_server_client_get_port(&client));

    struct httpserver_client httpClient = {
      .socket = socket,
      .upgraded = false,
      .data = NULL
    };

    server->sockets[socket_index] = httpClient;

    struct _httpserver_connection_data *args = frequenc_safe_malloc(sizeof(struct _httpserver_connection_data));

    args->server = server;
    args->client = client;
    args->socket_index = server->sockets_length;
    args->callback = callback;
    args->websocket_callback = websocket_callback;
    args->disconnect_callback = disconnect_callback;

    struct cthreads_thread thread;
    struct cthreads_args cargs;
    cthreads_thread_create(&thread, NULL, listen_messages, args, &cargs);

    server->sockets_length++;
  }

  printf("[httpserver]: Accept failed: %d\n", socket);

  perror("[httpserver]: Accept failed");
}

void httpserver_set_socket_data(struct httpserver *server, int socket_index, void *data) {
  server->sockets[socket_index].data = data;
}

void *httpserver_get_socket_data(struct httpserver *server, int socket_index) {
  return server->sockets[socket_index].data;
}

void httpserver_init_response(struct httpserver_response *response, struct httpserver_header *headers, int length) {
  response->client = NULL;
  response->status = 200;
  response->headers_length = 0;
  response->headers_max_length = length;
  response->headers = headers;
  response->body = NULL;
}

void httpserver_set_response_socket(struct httpserver_response *response, struct csocket_server_client *client) {
  response->client = client;
}

void httpserver_set_response_status(struct httpserver_response *response, int status) {
  response->status = status;
}

void httpserver_set_response_header(struct httpserver_response *response, char *key, char *value) {
  if (response->headers_length == response->headers_max_length) {
    printf("[httpserver]: Headers length is greater than headers max length.\n");

    return;
  }

  response->headers[response->headers_length].key = key;
  response->headers[response->headers_length].value = value;

  response->headers_length++;
}

void httpserver_set_response_body(struct httpserver_response *response, char *body) {
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

size_t _calculate_response_length(struct httpserver_response *response) {
  size_t length = 0;
  
  length += snprintf(NULL, 0, "HTTP/1.1 %d %s\r\n", response->status, _getStatusText(response->status));

  for (int i = 0; i < response->headers_length; i++) {
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

void httpserver_send_response(struct httpserver_response *response) {
  httpserver_set_response_header(response, "Connection", "close");

  size_t length = _calculate_response_length(response);

  char *response_string = frequenc_safe_malloc(length + 1);

  snprintf(response_string, length, "HTTP/1.1 %d %s\r\n", response->status, _getStatusText(response->status));

  for (int i = 0; i < response->headers_length; i++) {
    strcat(response_string, response->headers[i].key);
    strcat(response_string, ": ");
    strcat(response_string, response->headers[i].value);
    strcat(response_string, "\r\n");
  }

  strcat(response_string, "\r\n");

  if (response->body != NULL) {
    strcat(response_string, response->body);
  }

  csocket_server_send(response->client, response_string, length);

  free(response_string);
}

void httpserver_upgrade_socket(struct httpserver *server, int socket_index) {
  server->sockets[socket_index].upgraded = true;
}
