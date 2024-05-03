#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>

#include "base64.h"
#include "libtstr.h"

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

  int buffer_length = _read_int(&buf) & ~(1 << 30);

  if (buffer_length != output_length - 4 - 1) {
    printf("[track]: Failed to decode track.\n - Reason: Track binary length doesn't match track set length.\n - Expected: %d\n - Actual: %d\n", buffer_length, output_length - 4 - 1);

    free(output);

    return -1;
  }

  int version = _read_byte(&buf);

  if (version != 1) {
    printf("[track]: Failed to decode track.\n - Reason: Unknown track version.\n - Expected: 1\n - Actual: %d\n", version);

    free(output);

    return -1;
  }

  if ((result->title = _read_utf(&buf)) == NULL) return -1;
  if ((result->author = _read_utf(&buf)) == NULL) return -1;
  if ((result->length = _read_long(&buf)) == 0) return -1;
  if ((result->identifier = _read_utf(&buf)) == NULL) return -1;
  result->is_stream = _read_byte(&buf) == 1;
  if ((result->uri = _read_utf(&buf)) == NULL) return -1;
  if (_read_byte(&buf)) {
    if ((result->artwork_url = _read_utf(&buf)) == NULL) return -1;
  } else {
    result->artwork_url = NULL;
  }
  if (_read_byte(&buf)) {
    if ((result->isrc = _read_utf(&buf)) == NULL) return -1;
  } else {
    result->isrc = NULL;
  }
  if ((result->source_name = _read_utf(&buf)) == NULL) return -1;

  if (buf.position != output_length - 1) {
    printf("[track]: Failed to decode track.\n - Reason: Track binary length doesn't match the end of the sequence of reads.\n - Expected: %d\n - Actual: %d\n", buf.position, output_length - 1);

    free(output);

    return -1;
  }

  printf("[track]: Successfully decoded track.\n - Version: %d\n - Encoded: %s\n - Title: %s\n - Author: %s\n - Length: %lu\n - Identifier: %s\n - Is Stream: %s\n - URI: %s\n - Artwork URL: %s\n - ISRC: %s\n - Source Name: %s\n", result->version, track, result->title, result->author, result->length, result->identifier, result->is_stream ? "true" : "false", result->uri ? result->uri : "null", result->artwork_url ? result->artwork_url : "null", result->isrc ? result->isrc : "null", result->source_name);

  free(output);

  return 0;
}

void frequenc_free_track(struct frequenc_track_info *track) {
  frequenc_safe_free(track->title);
  frequenc_safe_free(track->author);
  frequenc_safe_free(track->identifier);
  frequenc_safe_free(track->uri);
  frequenc_safe_free(track->artwork_url);
  frequenc_safe_free(track->isrc);
  frequenc_safe_free(track->source_name);
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

void _write_utf(unsigned char *buffer, size_t *position, char *value, size_t len) {
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
  if (track->artwork_url != NULL) {
    size += 2;
    size += strlen(track->artwork_url);
  }

  /* ISRC */
  size += 1;
  if (track->isrc != NULL) {
    size += 2;
    size += strlen(track->isrc);
  }

  /* Source Name */
  size += 2;
  size += strlen(track->source_name);

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
  _write_byte(buffer, &position, track->is_stream ? 1 : 0);
  _write_utf(buffer, &position, track->uri);
  _write_byte(buffer, &position, track->artwork_url ? 1 : 0);
  if (track->artwork_url != NULL) _write_utf(buffer, &position, track->artwork_url);
  _write_byte(buffer, &position, track->isrc ? 1 : 0);
  if (track->isrc != NULL) _write_utf(buffer, &position, track->isrc);
  _write_utf(buffer, &position, track->source_name);

  _internal_write_int(buffer, 0, ((uint32_t)position - 4) | (1 << 30));

  int base64Length = b64_encoded_size(position);

  *result = frequenc_safe_malloc((base64Length + 1) * sizeof(char));
  b64_encode(buffer, *result, position);
  (*result)[base64Length] = '\0'; 
 
  free(buffer);

  return base64Length;
}

void frequenc_track_info_to_json(struct frequenc_track_info *track_info, char *encoded, struct pjsonb *track_json, bool unique) {
  if (unique == false) pjsonb_enter_object(track_json, NULL);

  pjsonb_set_string(track_json, "encoded", encoded);

  pjsonb_enter_object(track_json, "info");

  pjsonb_set_string(track_json, "title", track_info->title);
  pjsonb_set_string(track_json, "author", track_info->author);
  pjsonb_set_int(track_json, "length", track_info->length);
  pjsonb_set_string(track_json, "identifier", track_info->identifier);
  pjsonb_set_bool(track_json, "is_stream", track_info->is_stream);
  pjsonb_set_string(track_json, "uri", track_info->uri);
  pjsonb_set_if(track_json, string, track_info->artwork_url != NULL, "artwork_url", track_info->artwork_url);
  pjsonb_set_if(track_json, string, track_info->isrc != NULL, "isrc", track_info->isrc);
  pjsonb_set_string(track_json, "source_name", track_info->source_name);

  pjsonb_leave_object(track_json);

  if (unique == false) pjsonb_leave_object(track_json);
}

void frequenc_partial_track_info_to_json(struct frequenc_track_info *track_info, struct pjsonb *track_json) {
  pjsonb_enter_object(track_json, NULL);

  pjsonb_set_string(track_json, "title", track_info->title);
  pjsonb_set_string(track_json, "author", track_info->author);
  pjsonb_set_int(track_json, "length", track_info->length);
  pjsonb_set_string(track_json, "identifier", track_info->identifier);
  pjsonb_set_bool(track_json, "is_stream", track_info->is_stream);
  pjsonb_set_string(track_json, "uri", track_info->uri);
  pjsonb_set_if(track_json, string, track_info->artwork_url != NULL, "artwork_url", track_info->artwork_url);
  pjsonb_set_if(track_json, string, track_info->isrc != NULL, "isrc", track_info->isrc);
  pjsonb_set_string(track_json, "source_name", track_info->source_name);

  pjsonb_leave_object(track_json);
}

int frequenc_json_to_track_info(struct frequenc_track_info *track_info, jsmnf_pair *pairs, char *json, char *path[], int pathLen, int pathSize) {
  path[pathLen] = "title";
  jsmnf_pair *title = jsmnf_find_path(pairs, json, path, pathSize);
  if (title == NULL) return -1;

  char *title_str = frequenc_safe_malloc(title->v.len + 1);
  frequenc_fast_copy(json + title->v.pos, title_str, title->v.len);

  path[pathLen] = "author";
  jsmnf_pair *author = jsmnf_find_path(pairs, json, path, pathSize);
  if (author == NULL) return -1;

  char *author_str = frequenc_safe_malloc(author->v.len + 1);
  frequenc_fast_copy(json + author->v.pos, author_str, author->v.len);

  path[pathLen] = "length";
  jsmnf_pair *length = jsmnf_find_path(pairs, json, path, pathSize);
  if (length == NULL) return -1;

  char *length_str = frequenc_safe_malloc(length->v.len + 1);
  frequenc_fast_copy(json + length->v.pos, length_str, length->v.len);
  long length_num = strtol(length_str, NULL, 10);
  free(length_str);

  path[pathLen] = "identifier";
  jsmnf_pair *identifier = jsmnf_find_path(pairs, json, path, pathSize);
  if (identifier == NULL) return -1;

  char *identifier_str = frequenc_safe_malloc(identifier->v.len + 1);
  frequenc_fast_copy(json + identifier->v.pos, identifier_str, identifier->v.len);

  path[pathLen] = "is_stream";
  jsmnf_pair *is_stream = jsmnf_find_path(pairs, json, path, pathSize);
  if (is_stream == NULL) return -1;
  bool is_stream_bool = json[is_stream->v.pos] == 't';

  path[pathLen] = "uri";
  jsmnf_pair *uri = jsmnf_find_path(pairs, json, path, pathSize);

  char *uri_str = frequenc_safe_malloc(uri->v.len + 1);
  if (uri_str == NULL) return -1;
  frequenc_fast_copy(json + uri->v.pos, uri_str, uri->v.len);

  path[pathLen] = "artwork_url";
  jsmnf_pair *artwork_url = jsmnf_find_path(pairs, json, path, pathSize);

  char *artwork_url_str = NULL;
  if (artwork_url != NULL) {
    artwork_url_str = frequenc_safe_malloc(artwork_url->v.len + 1);
    frequenc_fast_copy(json + artwork_url->v.pos, artwork_url_str, artwork_url->v.len);
  }

  path[pathLen] = "isrc";
  jsmnf_pair *isrc = jsmnf_find_path(pairs, json, path, pathSize);

  char *isrc_str = NULL;
  if (isrc != NULL) {
    isrc_str = frequenc_safe_malloc(isrc->v.len + 1);
    frequenc_fast_copy(json + isrc->v.pos, isrc_str, isrc->v.len);
  }

  path[pathLen] = "source_name";
  jsmnf_pair *source_name = jsmnf_find_path(pairs, json, path, pathSize);
  if (source_name == NULL) return -1;

  char *source_name_str = frequenc_safe_malloc(source_name->v.len + 1);
  frequenc_fast_copy(json + source_name->v.pos, source_name_str, source_name->v.len);

  *track_info = (struct frequenc_track_info) {
    .title = title_str,
    .author = author_str,
    .length = length_num,
    .identifier = identifier_str,
    .is_stream = is_stream_bool,
    .uri = uri_str,
    .artwork_url = artwork_url_str,
    .isrc = isrc_str,
    .source_name = source_name_str
  };

  return 0;
}

void frequenc_free_track_info(struct frequenc_track_info *track_info) {
  frequenc_safe_free(track_info->title);
  frequenc_safe_free(track_info->author);
  frequenc_safe_free(track_info->identifier);
  frequenc_safe_free(track_info->uri);
  frequenc_safe_free(track_info->artwork_url);
  frequenc_safe_free(track_info->isrc);
  frequenc_safe_free(track_info->source_name);
}
