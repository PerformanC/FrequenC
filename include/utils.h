#ifndef UTILS_H
#define UTILS_H

#define JSMN_HEADER
#include "jsmn.h"
#include "jsmn-find.h"
#include "track.h"

unsigned int frequenc_safe_seeding(void);

char *frequenc_generate_session_id(char *result);

void frequenc_fast_copy(char *src, char *dest, size_t size);

void frequenc_cleanup(void *pointer);

void *frequenc_safe_malloc(size_t size);

void *frequenc_safe_realloc(void *pointer, size_t size);

int frequenc_track_info_to_json(struct frequenc_track_info *trackInfo, char *encoded, char *result, size_t size);

int frequenc_partial_track_info_to_json(struct frequenc_track_info *trackInfo, char *result, size_t size);

int frequenc_json_to_track_info(struct frequenc_track_info *trackInfo, jsmnf_pair *pairs, char *json, char *path[], int pathLen, int pathSize);

void frequenc_free_track_info(struct frequenc_track_info *trackInfo);

void frequenc_stringify_int(int length, char *result);

#endif
