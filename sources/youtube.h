#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "libtstr.h"

#include "sources.h"

#if AUDIO_QUALITY == 1
#define FREQUENC_YOUTUBE_AUDIO_ITAG 251
#elif AUDIO_QUALITY == 2
#define FREQUENC_YOUTUBE_AUDIO_ITAG 250
#elif AUDIO_QUALITY == 3
#define FREQUENC_YOUTUBE_AUDIO_ITAG 249
#else
#define FREQUENC_YOUTUBE_AUDIO_ITAG 599
#endif

struct frequenc_youtube_client {
  char name[sizeof("ANDROID_MUSIC")];
  char version[sizeof("xx.xx.xx")];
};

void frequenc_youtube_search(struct tstr_string *result, char *query, int type);

int frequenc_youtube_get_stream(struct frequenc_audio_stream *stream, struct frequenc_track_info track_info);

#endif
