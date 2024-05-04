/*
  PerformanC's Discord Voice: Minimalistic Discord voice library for C.

  License available on: licenses/performanc.license
*/

#include <stdio.h>

#include "pjsonb.h"
#include "jsmn-find.h"
#include "cthreads.h"

#include "httpclient.h"
#include "websocket.h"
#include "utils.h"

#include "pdvoice.h"

#define PDVOICE_DISCORD_GW_VERSION 4

void pdvoice_init(struct pdvoice *connection) {
  connection->ws_connection_info = frequenc_safe_malloc(sizeof(struct pdvoice_ws_connection_info));
  connection->udp_connection_info = NULL;
  connection->udp_thread_data = NULL;
  connection->hb_thread_data = NULL;
  connection->mutex = frequenc_safe_malloc(sizeof(struct cthreads_mutex));
  cthreads_mutex_init(connection->mutex, NULL);
}

void pdvoice_update_state(struct pdvoice *connection, char *session_id) {
  connection->ws_connection_info->session_id = session_id;
}

void pdvoice_update_server(struct pdvoice *connection, char *endpoint, char *token) {
  connection->ws_connection_info->endpoint = endpoint;
  connection->ws_connection_info->token = token;
}

void _pdvoice_on_connect(struct httpclient_response *client, void *user_data) {
  struct pdvoice *connection = user_data;

  struct pjsonb jsonb;
  pjsonb_init(&jsonb, PJSONB_OBJECT);

  pjsonb_set_int(&jsonb, "op", 0);

  cthreads_mutex_lock(connection->mutex);

  pjsonb_enter_object(&jsonb, "d");
  pjsonb_set_string(&jsonb, "server_id", connection->guild_id, strlen(connection->guild_id));
  pjsonb_set_string(&jsonb, "user_id", connection->bot_id, strlen(connection->bot_id));
  pjsonb_set_string(&jsonb, "token", connection->ws_connection_info->token, strlen(connection->ws_connection_info->token));
  pjsonb_set_string(&jsonb, "session_id", connection->ws_connection_info->session_id, strlen(connection->ws_connection_info->session_id));
  pjsonb_leave_object(&jsonb);

  cthreads_mutex_unlock(connection->mutex);

  pjsonb_end(&jsonb);

  frequenc_send_text_ws_client(client, jsonb.string, jsonb.position);

  pjsonb_free(&jsonb);
}

void _pdvoice_on_close(struct httpclient_response *client, struct frequenc_ws_frame *message, void *user_data) {
  (void)client;

  struct pdvoice *connection = user_data;

  printf("[pdvoice]: Connection with Discord voice gateway has been closed: %d\n", message->close_code);

  pdvoice_free(connection);
}

void *_pdvoice_udp(void *data) {
  struct _pdvoice_udp_thread_data *thread_data = data;

  int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_socket == -1) {
    printf("[pdvoice]: Failed to create UDP socket. Check your firewall.\n");

    return NULL;
  }

  int enable = 1;
  if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
    printf("[pdvoice]: Failed to set UDP socket options.\n");

    return NULL;
  }

  struct sockaddr_in udp_server;
  udp_server.sin_family = AF_INET;
  udp_server.sin_addr = (struct in_addr){
    INADDR_ANY
  };
  cthreads_mutex_lock(thread_data->connection->mutex);

  udp_server.sin_port = htons(thread_data->connection->udp_connection_info->port);

  cthreads_mutex_unlock(thread_data->connection->mutex);

  if (bind(udp_socket, (struct sockaddr *)&udp_server, sizeof(udp_server)) == -1) {
    printf("[pdvoice]: Failed to bind UDP socket.\n");

    return NULL;
  }

  unsigned char discovery_buffer[74];

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    discovery_buffer[0] = 1 >> 8;
    discovery_buffer[1] = 1 & 0xFF;
  #else
    discovery_buffer[0] = 1 & 0xFF;
    discovery_buffer[1] = 1 >> 8;
  #endif

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    discovery_buffer[2] = 70 >> 8;
    discovery_buffer[3] = 70 & 0xFF;
  #else
    discovery_buffer[2] = 70 & 0xFF;
    discovery_buffer[3] = 70 >> 8;
  #endif

  cthreads_mutex_lock(thread_data->connection->mutex);

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    discovery_buffer[4] = thread_data->connection->udp_connection_info->ssrc >> 24;
    discovery_buffer[5] = thread_data->connection->udp_connection_info->ssrc >> 16;
    discovery_buffer[6] = thread_data->connection->udp_connection_info->ssrc >> 8;
    discovery_buffer[7] = thread_data->connection->udp_connection_info->ssrc & 0xFF;
  #else
    discovery_buffer[4] = thread_data->connection->udp_connection_info->ssrc & 0xFF;
    discovery_buffer[5] = thread_data->connection->udp_connection_info->ssrc >> 8;
    discovery_buffer[6] = thread_data->connection->udp_connection_info->ssrc >> 16;
    discovery_buffer[7] = thread_data->connection->udp_connection_info->ssrc >> 24;
  #endif

  struct sockaddr_in destination;
  destination.sin_family = AF_INET;
  destination.sin_port = htons(thread_data->connection->udp_connection_info->port);
  destination.sin_addr.s_addr = inet_addr(thread_data->connection->udp_connection_info->ip);

  cthreads_mutex_unlock(thread_data->connection->mutex);

  int sent = sendto(udp_socket, discovery_buffer, sizeof(discovery_buffer), 0, (struct sockaddr *)&destination, sizeof(destination));
  if (sent == -1) {
    perror("[pdvoice]: Failed to send discovery packet to UDP socket");

    return NULL;
  }

  char buffer[2048];
  struct sockaddr_in udp_client;
  socklen_t udp_client_length = sizeof(udp_client);

  int received = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&udp_client, &udp_client_length);
  if (received == -1) {
    perror("[pdvoice]: Failed to receive from UDP socket");

    return NULL;
  }

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t *type_ptr = (uint8_t *)&buffer[0];
    int type = (type_ptr[0] << 8) | type_ptr[1];
  #else
    uint8_t *type_ptr = (uint8_t *)&buffer[1];
    int type = (type_ptr[1] << 8) | type_ptr[0];
  #endif

  if (type != 2) {
    printf("[pdvoice]: Received an invalid packet from UDP socket.\n");

    return NULL;
  }

  char ip[(3 + 1 + 3 + 1 + 3) + 1];
  frequenc_fast_copy(buffer + 8, ip, 16);

  #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    uint8_t *port_ptr = (uint8_t *)&buffer[received - 2];
    int port = (port_ptr[0] << 8) | port_ptr[1];
  #else
    uint8_t *port_ptr = (uint8_t *)&buffer[received - 1];
    int port = (port_ptr[1] << 8) | port_ptr[0];
  #endif

  printf("[pdvoice]: Realized IP discovery: %.*s.xxx:%d\n", 6, ip, port);

  int buffer_length = snprintf(buffer, sizeof(buffer), "{\"op\":1,\"d\":{\"protocol\":\"udp\",\"data\":{\"address\":\"%s\",\"port\":%d,\"mode\":\"xsalsa20_poly1305_lite\"}}}", ip, port);

  cthreads_mutex_lock(thread_data->connection->mutex);

  frequenc_send_text_ws_client(thread_data->client, buffer, buffer_length);

  cthreads_mutex_unlock(thread_data->connection->mutex);

  while (1) {
    received = recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr *)&udp_client, &udp_client_length);
    if (received == -1) {
      perror("[pdvoice]: Failed to receive from UDP socket");

      return NULL;
    }

    printf("[pdvoice]: Received data from UDP server with Discord.\n");
  }

  return NULL;
}

void *_pdvoice_hb_interval(void *data) {
  struct _pdvoice_hb_thread_data *thread_data = data;

  char *buffer = "{\"op\":3,\"d\":null}";

  while (1) {
    frequenc_sleep(thread_data->interval);

    cthreads_mutex_lock(thread_data->connection->mutex);

    frequenc_send_text_ws_client(thread_data->client, buffer, sizeof(buffer) - 1);

    cthreads_mutex_unlock(thread_data->connection->mutex);
  }

  return NULL;
}

void _pdvoice_on_message(struct httpclient_response *client, struct frequenc_ws_frame *message, void *user_data) {
  struct pdvoice *connection = user_data;

  printf("[pdvoice]: Received message from Discord voice gateway: %.*s\n", (int)message->payload_length, message->buffer);

  jsmn_parser parser;
  jsmntok_t *tokens = NULL;
  unsigned num_tokens = 0;

  jsmn_init(&parser);
  int r = jsmn_parse_auto(&parser, message->buffer, message->payload_length, &tokens, &num_tokens);
  if (r <= 0) {
    free(tokens);

    printf("[pdvoice]: Failed to parse JSON: %d\n", r);

    goto bad_request;
  }

  jsmnf_loader loader;
  jsmnf_pair *pairs = NULL;
  unsigned num_pairs = 0;

  jsmnf_init(&loader);
  r = jsmnf_load_auto(&loader, message->buffer, tokens, num_tokens, &pairs, &num_pairs);
  if (r <= 0) {
    free(tokens);
    free(pairs);
    
    printf("[pdvoice]: Failed to load JSON: %d\n", r);

    goto bad_request;
  }

  jsmnf_pair *op_pair = jsmnf_find(pairs, message->buffer, "op", sizeof("op") - 1);

  char op_str[4];
  frequenc_fast_copy(message->buffer + op_pair->v.pos, op_str, op_pair->v.len);

  int op = atoi(op_str);

  switch (op) {
    case 2: {
      cthreads_mutex_lock(connection->mutex);

      connection->udp_connection_info = frequenc_safe_malloc(sizeof(struct pdvoice_udp_connection_info));

      char *path[] = { "d", "ssrc", NULL };
      jsmnf_pair *ssrc_pair = jsmnf_find_path(pairs, message->buffer, path, 2);

      char ssrc[32];
      frequenc_fast_copy(message->buffer + ssrc_pair->v.pos, ssrc, ssrc_pair->v.len);

      connection->udp_connection_info->ssrc = atoi(ssrc);

      path[1] = "ip";
      jsmnf_pair *ip_pair = jsmnf_find_path(pairs, message->buffer, path, 2);

      connection->udp_connection_info->ip = malloc(ip_pair->v.len + 1);
      frequenc_fast_copy(message->buffer + ip_pair->v.pos, connection->udp_connection_info->ip, ip_pair->v.len);

      path[1] = "port";
      jsmnf_pair *port_pair = jsmnf_find_path(pairs, message->buffer, path, 2);

      char port[8];
      frequenc_fast_copy(message->buffer + port_pair->v.pos, port, port_pair->v.len);

      connection->udp_connection_info->port = atoi(port);

      struct cthreads_args args;

      connection->udp_thread_data = frequenc_safe_malloc(sizeof(struct _pdvoice_udp_thread_data));
      connection->udp_thread_data->connection = connection;
      connection->udp_thread_data->client = client;

      cthreads_thread_create(&connection->udp_thread, NULL, _pdvoice_udp, connection->udp_thread_data, &args);
      cthreads_thread_detach(connection->udp_thread);

      cthreads_mutex_unlock(connection->mutex);

      break;
    }
    case 8: {
      char *path[] = { "d", "heartbeat_interval", NULL };
      jsmnf_pair *interval_pair = jsmnf_find_path(pairs, message->buffer, path, 2);

      char interval[32];
      frequenc_fast_copy(message->buffer + interval_pair->v.pos, interval, interval_pair->v.len);

      cthreads_mutex_lock(connection->mutex);

      connection->hb_thread_data = frequenc_safe_malloc(sizeof(struct _pdvoice_hb_thread_data));
      connection->hb_thread_data->connection = connection;
      connection->hb_thread_data->client = client;
      connection->hb_thread_data->interval = atoi(interval);

      struct cthreads_args args;
      cthreads_thread_create(&connection->hb_thread, NULL, _pdvoice_hb_interval, connection->hb_thread_data, &args);
      cthreads_thread_detach(connection->hb_thread);

      cthreads_mutex_unlock(connection->mutex);

      break;
    }
    case 20: {
      // TODO
    }
  }

  free(tokens);
  free(pairs);

  return;

  bad_request: {
    printf("[pdvoice]: Discord sent a bad request.\n");

    return;
  }
}

void *_pdvoice_connect(void *data) {
  struct pdvoice *connection = data;

  char path[4 + 1 + 1];
  snprintf(path, sizeof(path), "/?v=%d", PDVOICE_DISCORD_GW_VERSION);

  int i = 0;
  while (connection->ws_connection_info->endpoint[i] != '\0') {
    if (connection->ws_connection_info->endpoint[i] == ':') {
      connection->ws_connection_info->endpoint[i] = '\0';

      break;
    }

    i++;
  }

  struct httpclient_request_params params = {
    .host = connection->ws_connection_info->endpoint,
    .port = 443,
    .path = path,
    .method = "GET",
    .headers = (struct httpparser_header[4 + 1]) {
      {
        .key = "User-Agent",
        .value = "DiscordBot (https://github.com/PerformanC/FrequenC, 1.0.0)"
      }
    },
    .headers_length = 1,
    .body = NULL
  };

  struct frequenc_ws_cbs cbs = {
    .user_data = connection,
    .on_connect = _pdvoice_on_connect,
    .on_close = _pdvoice_on_close,
    .on_message = _pdvoice_on_message
  };

  connection->httpclient = frequenc_safe_malloc(sizeof(struct httpclient_response));
  if (frequenc_connect_ws_client(&params, connection->httpclient, &cbs) == -1) {
    printf("[pdvoice]: Failed to connect to Discord voice gateway.\n");

    return NULL;
  }

  return NULL;
}

int pdvoice_connect(struct pdvoice *connection) {
  struct cthreads_args args;

  cthreads_thread_create(&connection->ws_thread, NULL, _pdvoice_connect, connection, &args);
  cthreads_thread_detach(connection->ws_thread);

  return 0;
}

void pdvoice_free(struct pdvoice *connection) {
  if (connection->mutex == NULL || cthreads_mutex_trylock(connection->mutex) == -1) {
    printf("[pdvoice]: Failed to lock mutex.\n");

    return;
  }

  if (connection->udp_thread_data) {
    cthreads_thread_cancel(connection->udp_thread);

    free(connection->udp_thread_data);
    connection->udp_thread_data = NULL;
  }
  if (connection->hb_thread_data) {
    cthreads_thread_cancel(connection->hb_thread);

    free(connection->hb_thread_data);
    connection->hb_thread_data = NULL;
  }

  if ((void *)&connection->ws_thread != NULL) {
    cthreads_thread_cancel(connection->ws_thread);
  }

  if (connection->ws_connection_info != NULL) {
    free(connection->ws_connection_info);
    connection->ws_connection_info = NULL;
  }

  if (connection->udp_connection_info != NULL) {
    frequenc_safe_free(connection->udp_connection_info->ip);

    free(connection->udp_connection_info);
    connection->udp_connection_info = NULL;
  }

  if (connection->httpclient != NULL) {
    httpclient_shutdown(connection->httpclient);
    free(connection->httpclient);
  }

  /* TODO: add close for the websocket */

  cthreads_mutex_unlock(connection->mutex);
  cthreads_mutex_destroy(connection->mutex);
  frequenc_safe_free(connection->mutex);
}
