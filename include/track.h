#ifndef TRACK_H
#define TRACK_H

#include <stdint.h>

#include "libtstr.h"

#include "types.h"

struct frequenc_track_info {
  int version;
  char *title;
  char *author;
  uint64_t length;
  char *identifier;
  bool is_stream;
  char *uri;
  char *artwork_url;
  char *isrc;
  char *source_name;
};

int frequenc_decode_track(struct frequenc_track_info *result, const char *track);

void frequenc_free_track(struct frequenc_track_info *track);

int frequenc_encode_track(struct frequenc_track_info *track, char **result);

void frequenc_track_info_to_json(struct frequenc_track_info *track_info, char *encoded, struct pjsonb *track_json, bool unique);

void frequenc_partial_track_info_to_json(struct frequenc_track_info *track_info, struct pjsonb *track_json);

int frequenc_json_to_track_info(struct frequenc_track_info *track_info, jsmnf_pair *pairs, char *json, char *path[], int pathLen, int pathSize);

void frequenc_free_track_info(struct frequenc_track_info *track_info);

#endif
