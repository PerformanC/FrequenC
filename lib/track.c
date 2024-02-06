#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"
#include "utils.h"

#include "track.h"

struct DecodeClass {
  size_t position;
  unsigned char *buffer;
};

size_t changeBytes(struct DecodeClass *decodeClass, size_t bytes) {
  decodeClass->position += bytes;
  return decodeClass->position - bytes;
}

int8_t readByte(struct DecodeClass *decodeClass) {
  return (int8_t)decodeClass->buffer[changeBytes(decodeClass, 1)];
}

uint16_t readUnsignedShort(struct DecodeClass *decodeClass) {
  return decodeClass->buffer[changeBytes(decodeClass, 2)] << 8 | decodeClass->buffer[decodeClass->position - 1];
}

uint32_t readInt(struct DecodeClass *decodeClass) {
  return decodeClass->buffer[changeBytes(decodeClass, 4)] << 24 | decodeClass->buffer[decodeClass->position - 3] << 16 |
         decodeClass->buffer[decodeClass->position - 2] << 8 | decodeClass->buffer[decodeClass->position - 1];
}

uint64_t readLong(struct DecodeClass *decodeClass) {
  uint32_t lsb = readInt(decodeClass);
  uint32_t msb = readInt(decodeClass);

  return msb + lsb;
}

char *readUtf(struct DecodeClass *decodeClass) {
  uint16_t len = readUnsignedShort(decodeClass);
  size_t start = changeBytes(decodeClass, len);

  char *result = frequenc_safe_malloc((len + 1) * sizeof(char));

  memcpy(result, decodeClass->buffer + start, len);
  result[len] = '\0';

  return result;
}

int decodeTrack(struct frequenc_track_info *result, char *track) {
  int outputLength = b64_decoded_size(track, strlen(track)) + 1;
  unsigned char *output = frequenc_safe_malloc(outputLength * sizeof(unsigned char));
  memset(output, 0, outputLength);

  if (b64_decode(track, output, outputLength) == 0) {
    printf("[track]: Failed to decode track.\n - Reason: Invalid base64 string.\n - Input: %s\n", track);

    free(output);

    return -1;
  }
  output[outputLength - 1] = '\0';

  struct DecodeClass buf = { 0 };
  buf.position = 0;
  buf.buffer = output;

  int bufferLength = readInt(&buf);

  if ((bufferLength & ~(1 << 30)) != outputLength - 4 - 1) {
    printf("[track]: Failed to decode track.\n - Reason: Track binary length doesn't match track set length.\n - Expected: %d\n - Actual: %d\n", bufferLength & ~(1 << 30), outputLength - 4 - 1);

    free(output);

    return -1;
  }

  int version = (result->version = ((bufferLength & 0xC0000000U) >> 30 & 1) != 0 ? readByte(&buf) : 1);

  if (version != 3) {
    printf("[track]: Failed to decode track.\n - Reason: Unknown track version.\n - Expected: 3\n - Actual: %d\n", version);

    free(output);

    return -1;
  }

  if ((result->title = readUtf(&buf)) == NULL) return -1;
  if ((result->author = readUtf(&buf)) == NULL) return -1;
  if ((result->length = readLong(&buf)) == 0) return -1;
  if ((result->identifier = readUtf(&buf)) == NULL) return -1;
  result->isStream = readByte(&buf) == 1;
  if (readByte(&buf)) {
    if ((result->uri = readUtf(&buf)) == NULL) return -1;
  } else {
    result->uri = NULL;
  }
  if (readByte(&buf)) {
    if ((result->artworkUrl = readUtf(&buf)) == NULL) return -1;
  } else {
    result->artworkUrl = NULL;
  }
  if (readByte(&buf)) {
    if ((result->isrc = readUtf(&buf)) == NULL) return -1;
  } else {
    result->isrc = NULL;
  }
  if ((result->sourceName = readUtf(&buf)) == NULL) return -1;
  result->position = readLong(&buf);

  printf("[track]: Successfully decoded track.\n - Title: %s\n - Author: %s\n - Length: %lu\n - Identifier: %s\n - Is Stream: %s\n - URI: %s\n - Artwork URL: %s\n - ISRC: %s\n - Source Name: %s\n - Position: %lu\n", result->title, result->author, result->length, result->identifier, result->isStream ? "true" : "false", result->uri ? result->uri : "null", result->artworkUrl ? result->artworkUrl : "null", result->isrc ? result->isrc : "null", result->sourceName, result->position);
  printf("[track]: Successfully decoded track.\n - Version: %d\n - Encoded: %s\n", result->version, track);

  free(output);

  return 0;
}

void freeTrack(struct frequenc_track_info *track) {
  if (track->title != NULL) free(track->title);
  if (track->author != NULL) free(track->author);
  if (track->identifier != NULL) free(track->identifier);
  if (track->uri != NULL) free(track->uri);
  if (track->artworkUrl != NULL) free(track->artworkUrl);
  if (track->isrc != NULL) free(track->isrc);
  if (track->sourceName != NULL) free(track->sourceName);
}

void writeByte(unsigned char *buffer, size_t *position, int8_t value) {
  buffer[*position] = value;
  *position += 1;
}

void writeUnsignedShort(unsigned char *buffer, size_t *position, uint16_t value) {
  buffer[*position] = value >> 8;
  buffer[*position + 1] = value & 0xFF;
  *position += 2;
}

void _writeInt(unsigned char *buffer, size_t position, uint32_t value) {
  buffer[position] = value >> 24;
  buffer[position + 1] = value >> 16;
  buffer[position + 2] = value >> 8;
  buffer[position + 3] = value & 0xFF;
}

void writeInt(unsigned char *buffer, size_t *position, uint32_t value) {
  _writeInt(buffer, *position, value);
  *position += 4;
}

void writeLong(unsigned char *buffer, size_t *position, uint64_t value) {
  writeInt(buffer, position, (uint32_t)(value >> 32));
  writeInt(buffer, position, (uint32_t)(value & 0xFFFFFFFF));
}

void writeUtf(unsigned char *buffer, size_t *position, char *value) {
  size_t len = strlen(value);

  writeUnsignedShort(buffer, position, len);
  memcpy(buffer + *position, value, len);

  *position += len;
}

int encodeTrack(struct frequenc_track_info *track, char **result) {
  size_t position = 4;
  unsigned char *buffer = frequenc_safe_malloc(1024 * sizeof(unsigned char));

  writeByte(buffer, &position, 3);
  writeUtf(buffer, &position, track->title);
  writeUtf(buffer, &position, track->author);
  writeLong(buffer, &position, track->length);
  writeUtf(buffer, &position, track->identifier);
  writeByte(buffer, &position, track->isStream ? 1 : 0);
  writeByte(buffer, &position, track->uri ? 1 : 0);
  if (track->uri != NULL) writeUtf(buffer, &position, track->uri);
  writeByte(buffer, &position, track->artworkUrl ? 1 : 0);
  if (track->artworkUrl != NULL) writeUtf(buffer, &position, track->artworkUrl);
  writeByte(buffer, &position, track->isrc ? 1 : 0);
  if (track->isrc != NULL) writeUtf(buffer, &position, track->isrc);
  writeUtf(buffer, &position, track->sourceName);
  writeLong(buffer, &position, track->position);

  _writeInt(buffer, 0, ((uint32_t)position - 4) | (1 << 30));

  int base64Length = b64_encoded_size(position);

  *result = frequenc_safe_malloc((base64Length + 1) * sizeof(char));
  b64_encode(buffer, *result, position);
  (*result)[base64Length] = '\0'; 
 
  free(buffer);

  return base64Length;
}

