#include <string.h>

#include "track.h"
#include "youtube.h"

#include "sources.h"

int frequenc_search(struct tstr_string *result, char *query) {
  if (strncmp(query, "ytsearch:", sizeof("ytsearch:") - 1) == 0) {
    frequenc_youtube_search(result, query + (sizeof("ytsearch:") - 1), 0);

    return 0;
  }

  return -1;
}

int frequenc_get_stream(struct frequenc_audio_stream *stream, struct frequenc_track_info track_info) {
  if (track_info.source_name.length == 0) return -1;

  if (strncmp(track_info.source_name.string, "YouTube", track_info.source_name.length) == 0)
    return frequenc_youtube_get_stream(stream, track_info);

  return -1;
}
