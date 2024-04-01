#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include "base64.h"
#include "utils.h"

#include "track.h"

struct _decoder_class {
  int position;
  unsigned char *buffer;
};

int _change_bytes(struct _decoder_class *decoder_class, int bytes) {
  decoder_class->position += bytes;
  return decoder_class->position - bytes;
}

int8_t _read_byte(struct _decoder_class *decoder_class) {
  return (int8_t)decoder_class->buffer[_change_bytes(decoder_class, 1)];
}

uint16_t _read_unsigned_short(struct _decoder_class *decoder_class) {
  return decoder_class->buffer[_change_bytes(decoder_class, 2)] << 8 | decoder_class->buffer[decoder_class->position - 1];
}

uint32_t _read_int(struct _decoder_class *decoder_class) {
  return decoder_class->buffer[_change_bytes(decoder_class, 4)] << 24 | decoder_class->buffer[decoder_class->position - 3] << 16 |
         decoder_class->buffer[decoder_class->position - 2] << 8 | decoder_class->buffer[decoder_class->position - 1];
}

uint64_t _read_long(struct _decoder_class *decoder_class) {
  uint32_t lsb = _read_int(decoder_class);
  uint32_t msb = _read_int(decoder_class);

  return msb + lsb;
}

char *_read_utf(struct _decoder_class *decoder_class) {
  uint16_t len = _read_unsigned_short(decoder_class);
  size_t start = _change_bytes(decoder_class, len);

  char *result = frequenc_safe_malloc((len + 1) * sizeof(char));

  memcpy(result, decoder_class->buffer + start, len);
  result[len] = '\0';

  return result;
}

int frequenc_decode_track(struct frequenc_track_info *result, const char *track) {
  int output_length = b64_decoded_size(track, strlen(track)) + 1;
  unsigned char *output = frequenc_safe_malloc(output_length * sizeof(unsigned char));
  memset(output, 0, output_length);

  if (b64_decode(track, output, output_length) == 0) {
    printf("[track]: Failed to decode track.\n - Reason: Invalid base64 string.\n - Input: %s\n", track);

    free(output);

    return -1;
  }
  output[output_length - 1] = '\0';

  struct _decoder_class buf = { 0 };
  buf.position = 0;
  buf.buffer = output;

  int buffer_length = _read_int(&buf);

  if ((buffer_length & ~(1 << 30)) != output_length - 4 - 1) {
    printf("[track]: Failed to decode track.\n - Reason: Track binary length doesn't match track set length.\n - Expected: %d\n - Actual: %d\n", buffer_length & ~(1 << 30), output_length - 4 - 1);

    free(output);

    return -1;
  }

  int version = (result->version = ((buffer_length & 0xC0000000U) >> 30 & 1) != 0 ? _read_byte(&buf) : 1);

  if (version != 1) {
    printf("[track]: Failed to decode track.\n - Reason: Unknown track version.\n - Expected: 1\n - Actual: %d\n", version);

    free(output);

    return -1;
  }

  if ((result->title = _read_utf(&buf)) == NULL) return -1;
  if ((result->author = _read_utf(&buf)) == NULL) return -1;
  if ((result->length = _read_long(&buf)) == 0) return -1;
  if ((result->identifier = _read_utf(&buf)) == NULL) return -1;
  result->isStream = _read_byte(&buf) == 1;
  if (_read_byte(&buf)) {
    if ((result->uri = _read_utf(&buf)) == NULL) return -1;
  } else {
    result->uri = NULL;
  }
  if (_read_byte(&buf)) {
    if ((result->artworkUrl = _read_utf(&buf)) == NULL) return -1;
  } else {
    result->artworkUrl = NULL;
  }
  if (_read_byte(&buf)) {
    if ((result->isrc = _read_utf(&buf)) == NULL) return -1;
  } else {
    result->isrc = NULL;
  }
  if ((result->sourceName = _read_utf(&buf)) == NULL) return -1;
  result->position = _read_long(&buf);

  if (buf.position != output_length - 1) {
    printf("[track]: Failed to decode track.\n - Reason: Track binary length doesn't match the end of the sequence of reads.\n - Expected: %d\n - Actual: %d\n", buf.position, output_length - 1);

    free(output);

    return -1;
  }

  printf("[track]: Successfully decoded track.\n - Version: %d\n - Encoded: %s\n - Title: %s\n - Author: %s\n - Length: %lu\n - Identifier: %s\n - Is Stream: %s\n - URI: %s\n - Artwork URL: %s\n - ISRC: %s\n - Source Name: %s\n - Position: %lu\n", result->version, track, result->title, result->author, result->length, result->identifier, result->isStream ? "true" : "false", result->uri ? result->uri : "null", result->artworkUrl ? result->artworkUrl : "null", result->isrc ? result->isrc : "null", result->sourceName, result->position);

  free(output);

  return 0;
}

void frequenc_free_track(struct frequenc_track_info *track) {
  frequenc_free_nullable(track->title);
  frequenc_free_nullable(track->author);
  frequenc_free_nullable(track->identifier);
  frequenc_free_nullable(track->uri);
  frequenc_free_nullable(track->artworkUrl);
  frequenc_free_nullable(track->isrc);
  frequenc_free_nullable(track->sourceName);
}

void _write_byte(unsigned char *buffer, size_t *position, int8_t value) {
  buffer[*position] = value;
  *position += 1;
}

void _write_unsigned_short(unsigned char *buffer, size_t *position, uint16_t value) {
  buffer[*position] = value >> 8;
  buffer[*position + 1] = value & 0xFF;
  *position += 2;
}

void _internal_write_int(unsigned char *buffer, size_t position, uint32_t value) {
  buffer[position] = value >> 24;
  buffer[position + 1] = value >> 16;
  buffer[position + 2] = value >> 8;
  buffer[position + 3] = value & 0xFF;
}

void _write_int(unsigned char *buffer, size_t *position, uint32_t value) {
  _internal_write_int(buffer, *position, value);
  *position += 4;
}

void _write_long(unsigned char *buffer, size_t *position, uint64_t value) {
  _write_int(buffer, position, (uint32_t)(value >> 32));
  _write_int(buffer, position, (uint32_t)(value & 0xFFFFFFFF));
}

void _write_utf(unsigned char *buffer, size_t *position, char *value) {
  size_t len = strlen(value);

  _write_unsigned_short(buffer, position, len);
  memcpy(buffer + *position, value, len);

  *position += len;
}

size_t _calculate_track_size(struct frequenc_track_info *track) {
  size_t size = 4;

  /* Version */
  size += 1;

  /* Title */
  size += 2;
  size += strlen(track->title);

  /* Author */
  size += 2;
  size += strlen(track->author);

  /* Length */
  size += 8;

  /* Identifier */
  size += 2;
  size += strlen(track->identifier);

  /* Is Stream */
  size += 1;

  /* URI */
  size += 1;
  if (track->uri != NULL) {
    size += 2;
    size += strlen(track->uri);
  }

  /* Artwork URL */
  size += 1;
  if (track->artworkUrl != NULL) {
    size += 2;
    size += strlen(track->artworkUrl);
  }

  /* ISRC */
  size += 1;
  if (track->isrc != NULL) {
    size += 2;
    size += strlen(track->isrc);
  }

  /* Source Name */
  size += 2;
  size += strlen(track->sourceName);

  /* Position */
  size += 8;

  return size;
}

int frequenc_encode_track(struct frequenc_track_info *track, char **result) {
  size_t position = 4;
  unsigned char *buffer = frequenc_safe_malloc(_calculate_track_size(track) * sizeof(unsigned char));

  _write_byte(buffer, &position, 1);
  _write_utf(buffer, &position, track->title);
  _write_utf(buffer, &position, track->author);
  _write_long(buffer, &position, track->length);
  _write_utf(buffer, &position, track->identifier);
  _write_byte(buffer, &position, track->isStream ? 1 : 0);
  _write_byte(buffer, &position, track->uri ? 1 : 0);
  if (track->uri != NULL) _write_utf(buffer, &position, track->uri);
  _write_byte(buffer, &position, track->artworkUrl ? 1 : 0);
  if (track->artworkUrl != NULL) _write_utf(buffer, &position, track->artworkUrl);
  _write_byte(buffer, &position, track->isrc ? 1 : 0);
  if (track->isrc != NULL) _write_utf(buffer, &position, track->isrc);
  _write_utf(buffer, &position, track->sourceName);
  _write_long(buffer, &position, track->position);

  _internal_write_int(buffer, 0, ((uint32_t)position - 4) | (1 << 30));

  int base64Length = b64_encoded_size(position);

  *result = frequenc_safe_malloc((base64Length + 1) * sizeof(char));
  b64_encode(buffer, *result, position);
  (*result)[base64Length] = '\0'; 
 
  free(buffer);

  return base64Length;
}

