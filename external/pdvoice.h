#ifndef PDVOICE_H
#define PDVOICE_H

#include "cthreads.h"

struct pdvoice_ws_connection_info {
  char *token;
  char *endpoint;
  char *session_id;
};

struct pdvoice_udp_connection_info {
  int ssrc;
  char *ip;
  int port;
};

struct pdvoice {
  struct pdvoice_ws_connection_info *ws_connection_info;
  struct pdvoice_udp_connection_info *udp_connection_info;
  struct cthreads_thread thread;
  char *bot_id;
  char *guild_id;
};

void pdvoice_init(struct pdvoice *connection);

void pdvoice_update_state(struct pdvoice *connection, char *session_id);

void pdvoice_update_server(struct pdvoice *connection, char *endpoint, char *token);

int pdvoice_connect(struct pdvoice *connection);

#endif
