#ifndef SOURCES_H
#define SOURCES_H

#include "libtstr.h"

#include "track.h"

enum frequenc_audio_stream_type {
  FREQUENC_AUDIO_STREAM_TYPE_WEBM_OPUS,
};

struct frequenc_audio_stream {
  struct tstr_string url;
  enum frequenc_audio_stream_type type;
};

int frequenc_search(struct tstr_string *result, char *query);

int frequenc_get_stream(struct frequenc_audio_stream *stream, struct frequenc_track_info track_info);

#endif
