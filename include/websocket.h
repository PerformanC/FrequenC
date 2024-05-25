#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "csocket-server.h"
#include "httpclient.h"

#include <stdint.h>

struct frequenc_ws_client {
  int socket;
};

struct frequenc_ws_frame {
  uint8_t opcode;
  int fin;
  char *buffer;
  size_t payload_length;
  unsigned short close_code;
};

struct frequenc_ws_message {
  struct csocket_server_client *client;
  uint8_t opcode;
  int fin;
  char *buffer;
  size_t payload_length;
};

struct frequenc_ws_cbs {
  void *user_data;
  void (*on_message)(struct httpclient_response *client, struct frequenc_ws_frame *message, void *user_data);
  void (*on_close)(struct httpclient_response *client, struct frequenc_ws_frame *message, void *user_data);
  void (*on_connect)(struct httpclient_response *client, void *user_data);
};

int frequenc_parse_ws_frame(struct frequenc_ws_frame *frame_header, char *buffer, int len);

void frequenc_gen_accept_key(char *key, char *output);

void frequenc_init_ws_response(struct frequenc_ws_message *response);

void frequenc_set_ws_response_socket(struct frequenc_ws_message *response, struct csocket_server_client *client);

void frequenc_set_ws_response_body(struct frequenc_ws_message *response, char *buffer, size_t length);

void frequenc_send_ws_response(struct frequenc_ws_message *response);

int frequenc_connect_ws_client(struct httpclient_request_params *request, struct httpclient_response *response, struct frequenc_ws_cbs *cbs);

int frequenc_send_text_ws_client(struct httpclient_response *response, char *message, size_t message_length);

#endif
