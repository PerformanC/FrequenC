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

int decodeTrack(struct frequenc_track_info *result, char *track);

void freeTrack(struct frequenc_track_info *track);

int encodeTrack(struct frequenc_track_info *track, char **result);

#endif
