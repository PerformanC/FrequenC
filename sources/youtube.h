#ifndef YOUTUBE_H
#define YOUTUBE_H

#include "libtstr.h"

struct frequenc_youtube_client {
  char name[sizeof("ANDROID_MUSIC")];
  char version[sizeof("xx.xx.xx")];
};

struct tstr_string frequenc_youtube_search(char *query, int type);

#endif
