#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sha1.h"
#include "base64.h"
#include "csocket-server.h"
#include "tcplimits.h"
#include "csocket-client.h"

#include "httpclient.h"
#include "utils.h"

#include "websocket.h"

int frequenc_parse_ws_frame(struct frequenc_ws_frame *frame_header, char *buffer, int len) {
  int start_index = 2;

  if (len < 2) {
    perror("[websocket]: Frame is too short");

    return -1;
  }

  uint8_t opcode = buffer[0] & 15;
  bool fin = (buffer[0] & 128) == 128;
  bool is_masked = (buffer[1] & 128) == 128;
  size_t payload_length = buffer[1] & 127;

  char mask[5];
  if ((buffer[1] & 128) == 128) frequenc_fast_copy(&buffer[2], mask, 4);

  if (payload_length == 126) {
    start_index += 2;

    if (len < start_index) {
      perror("[websocket]: Frame is too short");

      return -1;
    }

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

    if (len < start_index) {
      perror("[websocket]: Frame is too short");

      return -1;
    }

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

    if ((size_t)len < (size_t)start_index + payload_length) {
      perror("[websocket]: Frame is too short");

      return -1;
    }

    for (size_t i = 0; i < payload_length; ++i) {
      buffer[(size_t)start_index + i] ^= mask[i % 4];
    }
  }

  frame_header->opcode = opcode;
  frame_header->fin = fin;
  frame_header->buffer = &buffer[start_index];
  frame_header->payload_length = payload_length;

  return 0;
}

void frequenc_gen_accept_key(char *key, char *output) {
  char *magic_string = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  SHA1C_CTX sha;
  char results[20];

  char concatenated_string[24 + 36 + 1];
  int concatenated_string_length = snprintf(concatenated_string, sizeof(concatenated_string), "%s%s", key, magic_string);

  SHA1CInit(&sha);
  SHA1CUpdate(&sha, (const unsigned char *)concatenated_string, (uint32_t)concatenated_string_length);
  SHA1CFinal((unsigned char *)results, &sha);

  b64_encode((unsigned char *)results, output, sizeof(results));
  output[28] = '\0';
}

static void _frequenc_key_to_base64(char *key, char *output) {
  char results[20];

  SHA1C_CTX sha;

  SHA1CInit(&sha);
  SHA1CUpdate(&sha, (const unsigned char *)key, (uint32_t)strlen(key));
  SHA1CFinal((unsigned char *)results, &sha);

  b64_encode((unsigned char *)results, output, sizeof(results));
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

  unsigned int mask[] = { 0, 0, 0, 0 };
  
  srand(frequenc_safe_seeding());

  mask[0] = (unsigned int)rand();
  mask[1] = (unsigned int)rand();
  mask[2] = (unsigned int)rand();
  mask[3] = (unsigned int)rand();

  payload_start_index += 4;

  if (response->payload_length >= 65536) payload_start_index += 8;
  else if (response->payload_length > 125) payload_start_index += 2;

  unsigned char *buffer = frequenc_safe_malloc(payload_start_index + response->payload_length * sizeof(unsigned char));

  buffer[0] = response->opcode | 128;

  if (response->payload_length >= 65536) {
    buffer[2] = buffer[3] = 0;
    buffer[4] = (unsigned char)(response->payload_length >> 56);
    buffer[5] = (unsigned char)(response->payload_length >> 48);
    buffer[6] = (unsigned char)(response->payload_length >> 40);
    buffer[7] = (unsigned char)(response->payload_length >> 32);
    buffer[8] = (unsigned char)(response->payload_length >> 24);
    buffer[9] = (unsigned char)(response->payload_length >> 16);
    buffer[10] = (unsigned char)(response->payload_length >> 8);
    buffer[11] = (unsigned char)(response->payload_length & 0xFF);
  } else if (response->payload_length > 125) {
    buffer[2] = (unsigned char)(response->payload_length >> 8);
    buffer[3] = (unsigned char)(response->payload_length & 0xFF);
  } else {
    buffer[1] = (unsigned char)response->payload_length;
  }

  memcpy(buffer + payload_start_index, response->buffer, response->payload_length);

  buffer[1] |= 128;
  buffer[payload_start_index - 4] = (unsigned char)mask[0];
  buffer[payload_start_index - 3] = (unsigned char)mask[1];
  buffer[payload_start_index - 2] = (unsigned char)mask[2];
  buffer[payload_start_index - 1] = (unsigned char)mask[3];

  for (size_t i = 0; i < response->payload_length; ++i) {
    buffer[payload_start_index + i] ^= mask[i & 3];
  }

  if (csocket_server_send(response->client, (char *)buffer, payload_start_index + response->payload_length) == PCLL_ERROR) {
    perror("[websocket]: Failed to send message");

    frequenc_unsafe_free(buffer);

    return;
  }

  frequenc_unsafe_free(buffer);

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

  int key_int = rand();

  _frequenc_key_to_base64((char *)&key_int, request->headers[request->headers_length + 3].value);
  request->headers[request->headers_length + 3].value[24] = '\0';

  char accept_key[29];
  frequenc_gen_accept_key(request->headers[request->headers_length + 3].value, accept_key);

  snprintf(request->headers[request->headers_length + 3].key, (size_t)header_key_char_size, "Sec-WebSocket-Key");

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
    long len = csocket_client_recv(&response->connection, packet, TCPLIMITS_PACKET_SIZE);
    if (len == 0) {
      perror("[websocket]: Connection closed");

      goto exit;
    } else if (len == CSOCKET_CLIENT_ERROR) {
      perror("[websocket]: Failed to receive data");

      goto exit;
    }

    struct frequenc_ws_frame header = { 0 };
    frequenc_parse_ws_frame(&header, packet, (int)len);

    switch (header.opcode) {
      case 0: {
        if (continue_buffer == NULL) {
          continue_buffer = frequenc_safe_malloc(header.payload_length * sizeof(char));
          continue_buffer_length = header.payload_length;
        } else {
          continue_buffer = frequenc_safe_realloc(continue_buffer, (continue_buffer_length + header.payload_length) * sizeof(char));
          continue_buffer_length += header.payload_length;
        }

        frequenc_unsafe_fast_copy(header.buffer, continue_buffer + continue_buffer_length - header.payload_length, header.payload_length);

        if (header.fin) {
          struct frequenc_ws_frame continue_frame;
          continue_frame.opcode = 1;
          continue_frame.fin = 1;
          continue_frame.buffer = continue_buffer;
          continue_frame.payload_length = continue_buffer_length;

          cbs->on_message(response, &continue_frame, cbs->user_data);

          frequenc_safe_free(continue_buffer);
          continue_buffer_length = 0;
        }

        break;
      }
      case 1:
      case 2: {
        if (!header.fin) {
          continue_buffer = frequenc_safe_malloc(header.payload_length * sizeof(char));
          continue_buffer_length = header.payload_length;

          frequenc_unsafe_fast_copy(header.buffer, continue_buffer, header.payload_length);

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

        if (csocket_client_send(&response->connection, (char *)pong, 2) == PCLL_ERROR) {
          perror("[websocket]: Failed to send pong");

          goto exit;
        }

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
    frequenc_safe_free(continue_buffer);

    httpclient_shutdown(response);

    return 0;
  }
}

int frequenc_send_text_ws_client(struct httpclient_response *response, char *message, size_t message_length) {
  int payload_start_index = 2;

  unsigned int mask[] = { 0, 0, 0, 0 };
  
  srand(frequenc_safe_seeding());

  mask[0] = (unsigned int)rand();
  mask[1] = (unsigned int)rand();
  mask[2] = (unsigned int)rand();
  mask[3] = (unsigned int)rand();

  payload_start_index += 4;

  if (message_length >= 65536) payload_start_index += 8;
  else if (message_length > 125) payload_start_index += 2;

  unsigned char *buffer = frequenc_safe_malloc(((size_t)payload_start_index + message_length) * sizeof(unsigned char));

  buffer[0] = 1 | 128;

  if (message_length >= 65536) {
    buffer[1] = 127;

    buffer[2] = buffer[3] = 0;
    buffer[4] = (unsigned char)(message_length >> 56);
    buffer[5] = (unsigned char)(message_length >> 48);
    buffer[6] = (unsigned char)(message_length >> 40);
    buffer[7] = (unsigned char)(message_length >> 32);
    buffer[8] = (unsigned char)(message_length >> 24);
    buffer[9] = (unsigned char)(message_length >> 16);
    buffer[10] = (unsigned char)(message_length >> 8);
    buffer[11] = (unsigned char)(message_length & 0xFF);
  } else if (message_length > 125) {
    buffer[1] = 126;

    buffer[2] = (unsigned char)(message_length >> 8);
    buffer[3] = (unsigned char)(message_length & 0xFF);
  } else {
    buffer[1] = (unsigned char)message_length;
  }

  memcpy(buffer + payload_start_index, message, message_length);

  buffer[1] |= 128;
  buffer[payload_start_index - 4] = (unsigned char)mask[0];
  buffer[payload_start_index - 3] = (unsigned char)mask[1];
  buffer[payload_start_index - 2] = (unsigned char)mask[2];
  buffer[payload_start_index - 1] = (unsigned char)mask[3];

  for (size_t i = 0; i < message_length; ++i) {
    buffer[(size_t)payload_start_index + i] ^= (unsigned char)mask[i & 3];
  }

  if (csocket_client_send(&response->connection, (char *)buffer, (size_t)payload_start_index + message_length) == PCLL_ERROR) {
    perror("[websocket]: Failed to send message");

    frequenc_unsafe_free(buffer);

    return -1;
  }

  frequenc_unsafe_free(buffer);

  return 0;
}
