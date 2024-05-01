#ifndef PDVOICE_H
#define PDVOICE_H

#include "cthreads.h"

#include "httpclient.h"

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

struct _pdvoice_udp_thread_data {
  struct pdvoice *connection;
  struct httpclient_response *client;
};

struct _pdvoice_hb_thread_data {
  struct pdvoice *connection;
  struct httpclient_response *client;
  int interval;
};

struct pdvoice {
  struct pdvoice_ws_connection_info *ws_connection_info;
  struct pdvoice_udp_connection_info *udp_connection_info;
  struct cthreads_thread ws_thread;
  struct cthreads_thread udp_thread;
  struct cthreads_thread hb_thread;
  struct _pdvoice_udp_thread_data *udp_thread_data;
  struct _pdvoice_hb_thread_data *hb_thread_data;
  struct httpclient_response *httpclient;
  struct cthreads_mutex *mutex;
  char *bot_id;
  char *guild_id;
};

void pdvoice_init(struct pdvoice *connection);

void pdvoice_update_state(struct pdvoice *connection, char *session_id);

void pdvoice_update_server(struct pdvoice *connection, char *endpoint, char *token);

int pdvoice_connect(struct pdvoice *connection);

void pdvoice_free(struct pdvoice *connection);

#endif
