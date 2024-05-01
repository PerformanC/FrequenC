#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

/* TODO: Create C* library for use of numerous SSL libraries. */
#include <openssl/ssl.h>

#include "sha1.h"
#include "base64.h"
#include "csocket-server.h"
#include "tcplimits.h"

#include "httpclient.h"
#include "utils.h"

#include "websocket.h"

struct frequenc_ws_frame frequenc_parse_ws_frame(char *buffer) {
  size_t start_index = 2;

  uint8_t opcode = buffer[0] & 15;
  int fin = (buffer[0] & 128) == 128;
  int is_masked = (buffer[1] & 128) == 128;
  size_t payload_length = buffer[1] & 127;

  char mask[5];
  if ((buffer[1] & 128) == 128) frequenc_fast_copy(mask, &buffer[2], 4);

  if (payload_length == 126) {
    start_index += 2;

    /* TODO: how portable is __bswap_16? */
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      uint8_t *payload_length_ptr = (uint8_t *)&buffer[2];
      payload_length = (payload_length_ptr[0] << 8) | payload_length_ptr[1];
    #else
      uint8_t *payload_length_ptr = (uint8_t *)&buffer[3];
      payload_length = (payload_length_ptr[1] << 8) | payload_length_ptr[0];
    #endif
  } else if (payload_length == 127) {
    start_index += 8;

    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
      for (size_t i = 0; i < 8; i++) {
        payload_length = (payload_length << 8) | buffer[i + 2];
      }
    #else
      for (size_t i = 0; i < 8; i++) {
        payload_length = (payload_length << 8) | buffer[9 - i];
      }
    #endif
  }

  if (is_masked) {
    start_index += 4;

    for (size_t i = 0; i < payload_length; ++i) {
      buffer[start_index + i] ^= mask[i % 4];
    }
  }

  buffer[start_index + payload_length] = '\0';

  struct frequenc_ws_frame frame_header;
  frame_header.opcode = opcode;
  frame_header.fin = fin;
  frame_header.buffer = &buffer[start_index];
  frame_header.payload_length = payload_length;

  return frame_header;
}

void frequenc_gen_accept_key(char *key, char *output) {
  char *magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  SHA1C_CTX sha;
  char results[20 + 1];

  char concatenated_string[24 + 36 + 1];
  int concatenatedStringLength = snprintf(concatenated_string, sizeof(concatenated_string), "%s%s", key, magic_string);

  SHA1CInit(&sha);
  SHA1CUpdate(&sha, (const unsigned char *)concatenated_string, concatenatedStringLength);
  SHA1CFinal((unsigned char *)results, &sha);

  results[20] = '\0';

  b64_encode((unsigned char *)results, output, sizeof(results) - 1);
}

void frequenc_key_to_base64(char *key, char *output) {
  char results[20 + 1];

  SHA1C_CTX sha;

  SHA1CInit(&sha);
  SHA1CUpdate(&sha, (const unsigned char *)key, strlen(key));
  SHA1CFinal((unsigned char *)results, &sha);

  results[20] = '\0';

  b64_encode((unsigned char *)results, output, sizeof(results) - 1);
}

void frequenc_init_ws_response(struct frequenc_ws_message *response) {
  response->client = NULL;
  response->opcode = 1;
  response->fin = 1;
  response->buffer = NULL;
  response->payload_length = 0;
}

void frequenc_set_ws_response_socket(struct frequenc_ws_message *response, struct csocket_server_client *client) {
  response->client = client;
}

void frequenc_set_ws_response_body(struct frequenc_ws_message *response, char *buffer, size_t length) {
  response->buffer = buffer;
  response->payload_length = length;
}

void frequenc_send_ws_response(struct frequenc_ws_message *response) {
  size_t payload_start_index = 2;
  size_t payload_length = response->payload_length;

  unsigned int mask[] = { 0, 0, 0, 0 };
  
  srand(frequenc_safe_seeding());

  mask[0] = rand();
  mask[1] = rand();
  mask[2] = rand();
  mask[3] = rand();

  payload_start_index += 4;

  if (payload_length >= 65536) {
    payload_start_index += 8;
    payload_length = 127;
  } else if (payload_length > 125) {
    payload_start_index += 2;
    payload_length = 126;
  }

  unsigned char *buffer = frequenc_safe_malloc(payload_start_index + response->payload_length * sizeof(unsigned char));

  buffer[0] = response->opcode | 128;
  buffer[1] = payload_length;

  if (payload_length == 126) {
    buffer[2] = response->payload_length >> 8;
    buffer[3] = response->payload_length & 0xFF;
  } else if (payload_length == 127) {
    buffer[2] = buffer[3] = 0;
    buffer[4] = response->payload_length >> 56;
    buffer[5] = response->payload_length >> 48;
    buffer[6] = response->payload_length >> 40;
    buffer[7] = response->payload_length >> 32;
    buffer[8] = response->payload_length >> 24;
    buffer[9] = response->payload_length >> 16;
    buffer[10] = response->payload_length >> 8;
    buffer[11] = response->payload_length & 0xFF;
  }

  memcpy(buffer + payload_start_index, response->buffer, response->payload_length);

  buffer[1] |= 128;
  buffer[payload_start_index - 4] = mask[0];
  buffer[payload_start_index - 3] = mask[1];
  buffer[payload_start_index - 2] = mask[2];
  buffer[payload_start_index - 1] = mask[3];

  for (size_t i = 0; i < response->payload_length; ++i) {
    buffer[payload_start_index + i] ^= mask[i & 3];
  }

  csocket_server_send(response->client, (char *)buffer, payload_start_index + response->payload_length);

  free(buffer);

  return;
}

int frequenc_connect_ws_client(struct httpclient_request_params *request, struct httpclient_response *response, struct frequenc_ws_cbs *cbs) {
  int header_key_char_size = sizeof(request->headers[0].key);

  request->headers[request->headers_length] = (struct httpparser_header) {
    .key = "Upgrade",
    .value = "websocket"
  };
  
  request->headers[request->headers_length + 1] = (struct httpparser_header) {
    .key = "Connection",
    .value = "Upgrade"
  };

  request->headers[request->headers_length + 2] = (struct httpparser_header) {
    .key = "Sec-WebSocket-Version",
    .value = "13"
  };

  srand(frequenc_safe_seeding());

  size_t keyInt = (long)rand();

  frequenc_key_to_base64((char *)&keyInt, request->headers[request->headers_length + 3].value);
  request->headers[request->headers_length + 3].value[24] = '\0';

  char accept_key[29];
  frequenc_gen_accept_key(request->headers[request->headers_length + 3].value, accept_key);

  snprintf(request->headers[request->headers_length + 3].key, header_key_char_size, "Sec-WebSocket-Key");

  request->headers_length += 4;

  if (httpclient_request(request, response) == -1) {
    perror("[websocket]: Failed to connect to server");

    return -1;
  }

  httpclient_free(response);

  if (response->status != 101) {
    httpclient_shutdown(response);

    printf("[websocket]: Server didn't upgrade connection to WebSocket: %d\n", response->status);

    return -1;
  }

  cbs->on_connect(response, cbs->user_data);

  char packet[TCPLIMITS_PACKET_SIZE + 1];
  char *continue_buffer = NULL;
  size_t continue_buffer_length = 0;

  while (1) {
    int len = SSL_read(response->ssl, packet, TCPLIMITS_PACKET_SIZE);
    if (len == -1) {
      perror("[websocket]: Failed to receive HTTP response");

      goto exit;
    }

    if (len == 0) {
      perror("[websocket]: Connection closed");

      goto exit;
    }

    packet[len] = '\0';

    struct frequenc_ws_frame header = frequenc_parse_ws_frame(packet);

    switch (header.opcode) {
      case 0: {
        if (continue_buffer == NULL) {
          continue_buffer = frequenc_safe_malloc(header.payload_length * sizeof(char));
          continue_buffer_length = header.payload_length;
        } else {
          continue_buffer = frequenc_safe_realloc(continue_buffer, (continue_buffer_length + header.payload_length) * sizeof(char));
          continue_buffer_length += header.payload_length;
        }

        memcpy(continue_buffer + continue_buffer_length - header.payload_length, header.buffer, header.payload_length);

        if (header.fin) {
          struct frequenc_ws_frame continue_frame;
          continue_frame.opcode = 1;
          continue_frame.fin = 1;
          continue_frame.buffer = continue_buffer;
          continue_frame.payload_length = continue_buffer_length;

          cbs->on_message(response, &continue_frame, cbs->user_data);

          frequenc_cleanup(continue_buffer);
          continue_buffer_length = 0;
        }

        break;
      }
      case 1:
      case 2: {
        if (!header.fin) {
          continue_buffer = frequenc_safe_malloc(header.payload_length * sizeof(char));
          continue_buffer_length = header.payload_length;

          memcpy(continue_buffer, header.buffer, header.payload_length);

          break;
        }

        cbs->on_message(response, &header, cbs->user_data);

        break;
      }
      case 8: {
        /* TODO: how portable is __bswap_16? */
        #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
          uint8_t *close_code_ptr = (uint8_t *)&header.buffer[0];
          header.close_code = (close_code_ptr[0] << 8) | close_code_ptr[1];
        #else
          uint8_t *close_code_ptr = (uint8_t *)&buffer[1];
          header.close_code = (close_code_ptr[1] << 8) | close_code_ptr[0];
        #endif

        header.buffer += 2;

        cbs->on_close(response, &header, cbs->user_data);

        goto exit;
      }
      case 9: {
        int pong[] = { 0x8A, 0x00 };

        SSL_write(response->ssl, pong, 2);

        break;
      }
      case 0xA: {
        /* Pong -- Unused */

        break;
      }
      default: {
        perror("[websocket]: Unknown opcode");

        break;
      }
    }
  }

  exit: {
    if (continue_buffer != NULL) {
      frequenc_cleanup(continue_buffer);

      continue_buffer_length = 0;
    }

    httpclient_shutdown(response);

    return 0;
  }
}

int frequenc_send_text_ws_client(struct httpclient_response *response, char *message, size_t message_length) {
  int payload_start_index = 2;
  size_t payload_length = message_length;

  unsigned int mask[] = { 0, 0, 0, 0 };
  
  srand(frequenc_safe_seeding());

  mask[0] = rand();
  mask[1] = rand();
  mask[2] = rand();
  mask[3] = rand();

  payload_start_index += 4;

  if (payload_length >= 65536) {
    payload_start_index += 8;
    payload_length = 127;
  } else if (payload_length > 125) {
    payload_start_index += 2;
    payload_length = 126;
  }

  unsigned char *buffer = frequenc_safe_malloc(payload_start_index + message_length * sizeof(unsigned char));

  buffer[0] = 1 | 128;
  buffer[1] = payload_length;

  if (payload_length == 126) {
    buffer[2] = message_length >> 8;
    buffer[3] = message_length & 0xFF;
  } else if (payload_length == 127) {
    buffer[2] = buffer[3] = 0;
    buffer[4] = message_length >> 56;
    buffer[5] = message_length >> 48;
    buffer[6] = message_length >> 40;
    buffer[7] = message_length >> 32;
    buffer[8] = message_length >> 24;
    buffer[9] = message_length >> 16;
    buffer[10] = message_length >> 8;
    buffer[11] = message_length & 0xFF;
  }

  memcpy(buffer + payload_start_index, message, message_length);

  buffer[1] |= 128;
  buffer[payload_start_index - 4] = mask[0];
  buffer[payload_start_index - 3] = mask[1];
  buffer[payload_start_index - 2] = mask[2];
  buffer[payload_start_index - 1] = mask[3];

  for (size_t i = 0; i < message_length; ++i) {
    buffer[payload_start_index + i] ^= mask[i & 3];
  }

  SSL_write(response->ssl, buffer, payload_start_index + message_length);

  free(buffer);

  return 0;
}
