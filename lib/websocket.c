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

struct frequenc_ws_header frequenc_parse_ws_header(char *buffer) {
  size_t startIndex = 2;

  uint8_t opcode = buffer[0] & 15;
  int fin = (buffer[0] & 128) == 128;
  int isMasked = (buffer[1] & 128) == 128;
  size_t payloadLength = buffer[1] & 127;

  char mask[5];
  if ((buffer[1] & 128) == 128) {
    memcpy(mask, &buffer[2], 4);
    mask[4] = '\0';
  }

  if (payloadLength == 126) {
    startIndex += 2;
    payloadLength = (buffer[startIndex] << 8) | buffer[startIndex + 1];
  } else if (payloadLength == 127) {
    uint8_t buf[8];
    for (size_t i = 0; i < 8; ++i) {
      buf[i] = buffer[startIndex + i];
    }

    payloadLength = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) | ((uint64_t)buf[2] << 40) |
                    ((uint64_t)buf[3] << 32) | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                    ((uint64_t)buf[6] << 8) | buf[7];

    startIndex += 8;
  }

  if (isMasked) {
    startIndex += 4;

    for (size_t i = 0; i < payloadLength; ++i) {
      buffer[startIndex + i] ^= mask[i % 4];
    }
  }

  buffer[startIndex + payloadLength] = '\0';

  struct frequenc_ws_header frameHeader;
  frameHeader.opcode = opcode;
  frameHeader.fin = fin;
  frameHeader.buffer = &buffer[startIndex];
  frameHeader.payloadLength = payloadLength;

  return frameHeader;
}

void frequenc_gen_accept_key(char *key, char *output) {
  char *magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  SHA1C_CTX sha;
  char results[20 + 1];

  char concatenatedString[24 + 36 + 1];
  int concatenatedStringLength = snprintf(concatenatedString, sizeof(concatenatedString), "%s%s", key, magicString);

  SHA1CInit(&sha);
  SHA1CUpdate(&sha, (const unsigned char *)concatenatedString, concatenatedStringLength);
  SHA1CFinal((unsigned char *)results, &sha);

  results[20] = '\0';

  b64_encode((unsigned char *)results, output, sizeof(results) - 1);
}

void frequenc_init_ws_response(struct frequenc_ws_message *response) {
  response->client = NULL;
  response->opcode = 1;
  response->fin = 1;
  response->buffer = NULL;
  response->payloadLength = 0;
}

void frequenc_set_ws_response_socket(struct frequenc_ws_message *response, struct csocket_server_client *client) {
  response->client = client;
}

void frequenc_set_ws_response_body(struct frequenc_ws_message *response, char *buffer, size_t length) {
  response->buffer = buffer;
  response->payloadLength = length;
}

void frequenc_send_ws_response(struct frequenc_ws_message *response) {
  (void)response;
}

int frequenc_connect_ws_client(struct httpclient_request_params *request, struct httpclient_response *response, void (*onMessage)(struct httpclient_response *client, struct frequenc_ws_header *message), void (*onClose)(struct httpclient_response *client, struct frequenc_ws_header *message)) {
  int header_key_char_size = sizeof(request->headers[0].key);
  int header_value_char_size = sizeof(request->headers[0].value);

  request->headers[request->headersLength] = (struct httpparser_header) {
    .key = "Upgrade",
    .value = "websocket"
  };
  
  request->headers[request->headersLength + 1] = (struct httpparser_header) {
    .key = "Connection",
    .value = "Upgrade"
  };

  request->headers[request->headersLength + 2] = (struct httpparser_header) {
    .key = "Sec-WebSocket-Version",
    .value = "13"
  };

  srand(frequenc_safe_seeding());

  long keyInt = (long)rand();

  while (keyInt < 9999999999999999) {
    keyInt *= 10;
  }

  char key[16 + 1];
  snprintf(key, sizeof(key), "%ld", keyInt);

  char acceptKey[29];
  frequenc_gen_accept_key(key, acceptKey);

  snprintf(request->headers[request->headersLength + 3].key, header_key_char_size, "Sec-WebSocket-Key");
  snprintf(request->headers[request->headersLength + 3].value, header_value_char_size, "%s", key);

  request->headersLength += 4;

  if (httpclient_request(request, response) == -1) {
    perror("[websocket]: Failed to connect to server");

    return -1;
  }

  httpclient_free(response);

  if (response->status != 101) {
    httpclient_shutdown(response);

    perror("[websocket]: Server didn't upgrade connection to WebSocket");

    return -1;
  }

  char packet[TCPLIMITS_PACKET_SIZE + 1];
  char *continueBuffer = NULL;
  size_t continueBufferLength = 0;

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

    struct frequenc_ws_header header = frequenc_parse_ws_header(packet);

    printf("[websocket]: Received frame with opcode %d\n", header.opcode);

    switch (header.opcode) {
      case 0: {
        if (continueBuffer == NULL) {
          continueBuffer = frequenc_safe_malloc(header.payloadLength * sizeof(char));
          continueBufferLength = header.payloadLength;
        } else {
          continueBuffer = frequenc_safe_realloc(continueBuffer, (continueBufferLength + header.payloadLength) * sizeof(char));
          continueBufferLength += header.payloadLength;
        }

        memcpy(continueBuffer + continueBufferLength - header.payloadLength, header.buffer, header.payloadLength);

        if (header.fin) {
          struct frequenc_ws_header continueHeader;
          continueHeader.opcode = 1;
          continueHeader.fin = 1;
          continueHeader.buffer = continueBuffer;
          continueHeader.payloadLength = continueBufferLength;

          onMessage(response, &continueHeader);

          frequenc_cleanup(continueBuffer);
          continueBufferLength = 0;
        }

        break;
      }
      case 1:
      case 2: {
        if (!header.fin) {
          continueBuffer = frequenc_safe_malloc(header.payloadLength * sizeof(char));
          continueBufferLength = header.payloadLength;

          memcpy(continueBuffer, header.buffer, header.payloadLength);

          break;
        }

        onMessage(response, &header);

        break;
      }
      case 8: {
        unsigned short close_code = header.buffer[0] << 8 | header.buffer[1];

        header.buffer += 2;

        printf("[websocket]: Connection closed with code %d\n", close_code);
        onClose(response, &header);

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
    if (continueBuffer != NULL) {
      frequenc_cleanup(continueBuffer);
      continueBufferLength = 0;
    }

    httpclient_shutdown(response);

    return 0;
  }
}

int frequenc_send_text_ws_client(struct httpclient_response *response, char *message) {
  size_t messageLength = strlen(message);

  int payloadStartIndex = 2;
  size_t payloadLength = messageLength;

  unsigned int mask[] = { 0, 0, 0, 0 };
  
  // while (mask[0] == 0 && mask[1] == 0 && mask[2] == 0 && mask[3] == 0) {
    srand(frequenc_safe_seeding());

    mask[0] = rand();
    mask[1] = rand();
    mask[2] = rand();
    mask[3] = rand();
  // }

  payloadStartIndex += 4;

  if (payloadLength >= 65536) {
    payloadStartIndex += 8;
    payloadLength = 127;
  } else if (payloadLength > 125) {
    payloadStartIndex += 2;
    payloadLength = 126;
  }

  unsigned char *buffer = frequenc_safe_malloc(payloadStartIndex + messageLength * sizeof(unsigned char));

  buffer[0] = 1 | 128;
  buffer[1] = payloadLength;

  if (payloadLength == 126) {
    memcpy(buffer + 2, &messageLength, sizeof(uint16_t));
  } else if (payloadLength == 127) {
    buffer[2] = buffer[3] = 0;
    memcpy(buffer + 4, &messageLength, sizeof(uint64_t));
  }

  memcpy(buffer + payloadStartIndex, message, messageLength);

  buffer[1] |= 128;
  buffer[payloadStartIndex - 4] = mask[0];
  buffer[payloadStartIndex - 3] = mask[1];
  buffer[payloadStartIndex - 2] = mask[2];
  buffer[payloadStartIndex - 1] = mask[3];

  for (size_t i = 0; i < messageLength; ++i) {
    buffer[payloadStartIndex + i] ^= mask[i & 3];
  }

  SSL_write(response->ssl, buffer, payloadStartIndex + messageLength);

  free(buffer);

  return 0;
}
