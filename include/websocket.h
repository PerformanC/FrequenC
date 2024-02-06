#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "csocket-server.h"
#include "httpclient.h"

#include <stdint.h>

struct frequenc_ws_client {
  int socket;
};

struct frequenc_ws_header {
  uint8_t opcode;
  int fin;
  char *buffer;
  size_t payloadLength;
};

struct frequenc_ws_message {
  struct csocket_server_client *client;
  uint8_t opcode;
  int fin;
  char *buffer;
  size_t payloadLength;
};

struct frequenc_ws_header frequenc_parse_ws_header(char *buffer);

void frequenc_gen_accept_key(char *key, char *output);

void frequenc_init_ws_response(struct frequenc_ws_message *response);

void frequenc_set_ws_response_socket(struct frequenc_ws_message *response, struct csocket_server_client *client);

void frequenc_set_ws_response_body(struct frequenc_ws_message *response, char *buffer, size_t length);

void frequenc_send_ws_response(struct frequenc_ws_message *response);

int frequenc_connect_ws_client(struct httpclient_request_params *request, struct httpclient_response *response, void (*onMessage)(struct httpclient_response *client, struct frequenc_ws_header *message), void (*onClose)(struct httpclient_response *client, struct frequenc_ws_header *message));

int frequenc_send_text_ws_client(struct httpclient_response *response, char *message);

#endif
