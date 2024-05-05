#ifndef UTILS_H
#define UTILS_H

#define JSMN_HEADER
#define JSMN_STRICT /* Set strict mode for jsmn (JSON tokenizer) */
#include "jsmn.h"
#include "jsmn-find.h"
#include "pjsonb.h"

#include "track.h"
#include "types.h"

struct frequenc_client_info {
  char *name;
  char *version;
  char *bot_name;
};

void *frequenc_safe_malloc(size_t size);

void *frequenc_safe_realloc(void *pointer, size_t size);

void frequenc_safe_free(void *pointer);

void frequenc_unsafe_free(void *pointer);

void frequenc_fast_copy(const char *src, char *dest, size_t size);

void frequenc_unsafe_fast_copy(const char *src, char *dest, size_t size);

char *frequenc_strdup(const char *str, size_t size);

void frequenc_sleep(int ms);

unsigned int frequenc_safe_seeding(void);

char *frequenc_generate_session_id(char *result);

void frequenc_stringify_int(int length, char *result, size_t result_size);

int frequenc_parse_client_info(char *client_info, struct frequenc_client_info *result);

void frequenc_free_client_info(struct frequenc_client_info *client);

#endif
