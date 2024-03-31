#ifndef TRACK_H
#define TRACK_H

#include <stdint.h>

struct frequenc_track_info {
  int version;
  char *title;
  char *author;
  uint64_t length;
  char *identifier;
  int8_t isStream;
  char *uri;
  char *artworkUrl;
  char *isrc;
  char *sourceName;
  uint64_t position;
};

int frequenc_decode_track(struct frequenc_track_info *result, const char *track);

void frequenc_free_track(struct frequenc_track_info *track);

int frequenc_encode_track(struct frequenc_track_info *track, char **result);

#endif
