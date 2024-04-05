#ifndef UTILS_H
#define UTILS_H

#define JSMN_HEADER
#include "jsmn.h"
#include "jsmn-find.h"
#include "track.h"

struct frequenc_client_info {
  char *name;
  char *version;
  char *bot_name;
};

unsigned int frequenc_safe_seeding(void);

char *frequenc_generate_session_id(char *result);

void frequenc_fast_copy(char *src, char *dest, size_t size);

void frequenc_cleanup(void *pointer);

void frequenc_free_nullable(void *pointer);

void *frequenc_safe_malloc(size_t size);

void *frequenc_safe_realloc(void *pointer, size_t size);

int frequenc_track_info_to_json(struct frequenc_track_info *track_info, char *encoded, char *result, size_t size);

int frequenc_partial_track_info_to_json(struct frequenc_track_info *track_info, char *result, size_t size);

int frequenc_json_to_track_info(struct frequenc_track_info *track_info, jsmnf_pair *pairs, char *json, char *path[], int pathLen, int pathSize);

void frequenc_free_track_info(struct frequenc_track_info *track_info);

void frequenc_stringify_int(int length, char *result, size_t result_size);

int frequenc_parse_client_info(char *client_info, struct frequenc_client_info *result);

void frequenc_free_client_info(struct frequenc_client_info *client);

#endif
