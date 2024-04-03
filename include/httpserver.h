#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "csocket-server.h"

#include "websocket.h"
#include "httpparser.h"
#include "types.h"

struct httpserver {
  int *available_sockets;
  struct csocket_server server;
  size_t sockets_capacity;
  size_t sockets_length;
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
  int headers_length;
  int headers_max_length;
  struct httpserver_header *headers;
  char *body;
};

struct httpserver_client {
  int socket;
  bool upgraded;
  void *data;
};

void httpserver_start_server(struct httpserver *server);

void httpserver_stop_server(struct httpserver *server);

void httpserver_disconnect_client(struct csocket_server_client *client);

void httpserver_handle_request(struct httpserver *server, void (*callback)(struct csocket_server_client *client, int socket_index, struct httpparser_request *request), int (*websocket_callback)(struct csocket_server_client *client, struct frequenc_ws_frame *frame_header), void (*disconnect_callback)(struct csocket_server_client *client, int socket_index));

void httpserver_set_socket_data(struct httpserver *server, int socket_index, void *data);

void *httpserver_get_socket_data(struct httpserver *server, int socket_index);

void httpserver_init_response(struct httpserver_response *response, struct httpserver_header *headers, int length);

void httpserver_set_response_socket(struct httpserver_response *response, struct csocket_server_client *client);

void httpserver_set_response_status(struct httpserver_response *response, int status);

void httpserver_set_response_header(struct httpserver_response *response, char *key, char *value);

void httpserver_set_response_body(struct httpserver_response *response, char *body);

void httpserver_send_response(struct httpserver_response *response);

void httpserver_set_socket_data(struct httpserver *server, int socket, void *data);

void httpserver_upgrade_socket(struct httpserver *server, int socket_index);

#endif /* HTTPSERVER_H_ */
