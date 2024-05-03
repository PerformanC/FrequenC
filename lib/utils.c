#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if __linux__
  #include <sys/random.h>
#endif
#ifdef _WIN32
  #include <windows.h>
#else
  #if _POSIX_C_SOURCE >= 199309L
    #include <time.h>
  #else
    #include <unistd.h>
  #endif
#endif

#define JSMN_HEADER
#define JSMN_STRICT /* Set strict mode for jsmn (JSON tokenizer) */
#include "jsmn.h"
#include "jsmn-find.h"
#include "libtstr.h"
#include "pjsonb.h"

#include "track.h"
#include "types.h"

#include "utils.h"

unsigned int frequenc_safe_seeding(void) {
  unsigned int seed = 0;
  
  #ifdef _WIN32
    if (rand_s(&seed) != 0) {
      perror("[utils:Windows]: Failed to generate random seed");

      exit(1);
    }
  #elif __linux__
    if (getrandom(&seed, sizeof(seed), 0) == -1) {
      perror("[utils:Linux]: Failed to generate random seed");

      exit(1);
    }
  #elif ALLOW_UNSECURE_RANDOM
    printf("[utils:Unknown]: Warning! Using unsecure random seed.\n");

    seed = time(NULL);
  #else
    printf("[utils:Unknown]: Warning! Unsupported OS. Using /dev/urandom to generate random seed.\n");

    FILE *urandom = fopen("/dev/urandom", "rb");
    if (urandom == NULL) {
      perror("[utils:Unknown]: Failed to open /dev/urandom");

      exit(1);
    }

    if (fread(&seed, sizeof(seed), 1, urandom) != 1) {
      fclose(urandom);

      perror("[utils:Unknown]: Failed to read from /dev/urandom");

      exit(1);
    }

    fclose(urandom);
  #endif

  return seed;
}

char *frequenc_generate_session_id(char *result) {
  #if HARDCODED_SESSION_ID
    frequenc_fast_copy("FrequenC_Develop", result, 17);

    result[16] = '\0';

    return result;
  #endif

  char characters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

  srand(frequenc_safe_seeding());

  int i = 0;
  while (i < 16) {
    result[i] = characters[rand() % (sizeof(characters) - 1)];
    i++;
  }

  result[i] = '\0';

  return result;
}

void frequenc_fast_copy(const char *src, char *dest, size_t size) {
  memcpy(dest, src, size);
  dest[size] = '\0';
}

void frequenc_cleanup(void *pointer) {
  if (pointer != NULL) {
    free(pointer);

    pointer = NULL;
  }
}

void frequenc_free_nullable(void *pointer) {
  if (pointer) free(pointer);
}

void *frequenc_safe_malloc(size_t size) {
  void *pointer = malloc(size);

  if (pointer == NULL) {
    perror("[utils]: Failed to allocate memory");

    exit(1);
  }

  return pointer;
}

void *frequenc_safe_realloc(void *pointer, size_t size) {
  void *newPtr = realloc(pointer, size);

  if (newPtr == NULL) {
    perror("[utils]: Failed to reallocate memory");

    exit(1);
  }

  return newPtr;
}

char *frequenc_strdup(const char *str, size_t size) {
  size_t len = size == 0 ? strlen(str) : size;
  char *new_str = frequenc_safe_malloc(len + 1);
  frequenc_fast_copy(str, new_str, len);

  return new_str;
}

void frequenc_track_info_to_json(struct frequenc_track_info *track_info, char *encoded, struct pjsonb *track_json, bool unique) {
  if (unique == false) pjsonb_enter_object(track_json, NULL);

  pjsonb_set_string(track_json, "encoded", encoded);

  pjsonb_enter_object(track_json, "info");

  pjsonb_set_string(track_json, "title", track_info->title);
  pjsonb_set_string(track_json, "author", track_info->author);
  pjsonb_set_int(track_json, "length", track_info->length);
  pjsonb_set_string(track_json, "identifier", track_info->identifier);
  pjsonb_set_bool(track_json, "isStream", track_info->isStream);
  pjsonb_set_string(track_json, "uri", track_info->uri);
  pjsonb_set_if(track_json, string, track_info->artworkUrl != NULL, "artworkUrl", track_info->artworkUrl);
  pjsonb_set_if(track_json, string, track_info->isrc != NULL, "isrc", track_info->isrc);
  pjsonb_set_string(track_json, "sourceName", track_info->sourceName);

  pjsonb_leave_object(track_json);

  if (unique == false) pjsonb_leave_object(track_json);
}

void frequenc_partial_track_info_to_json(struct frequenc_track_info *track_info, struct pjsonb *track_json) {
  pjsonb_enter_object(track_json, NULL);

  pjsonb_set_string(track_json, "title", track_info->title);
  pjsonb_set_string(track_json, "author", track_info->author);
  pjsonb_set_int(track_json, "length", track_info->length);
  pjsonb_set_string(track_json, "identifier", track_info->identifier);
  pjsonb_set_bool(track_json, "isStream", track_info->isStream);
  pjsonb_set_string(track_json, "uri", track_info->uri);
  pjsonb_set_if(track_json, string, track_info->artworkUrl != NULL, "artworkUrl", track_info->artworkUrl);
  pjsonb_set_if(track_json, string, track_info->isrc != NULL, "isrc", track_info->isrc);
  pjsonb_set_string(track_json, "sourceName", track_info->sourceName);

  pjsonb_leave_object(track_json);
}

int frequenc_json_to_track_info(struct frequenc_track_info *track_info, jsmnf_pair *pairs, char *json, char *path[], int pathLen, int pathSize) {
  path[pathLen] = "title";
  jsmnf_pair *title = jsmnf_find_path(pairs, json, path, pathSize);
  if (title == NULL) return -1;

  char *title_str = frequenc_safe_malloc(title->v.len + 1);
  frequenc_fast_copy(json + title->v.pos, title_str, title->v.len);

  path[pathLen] = "author";
  jsmnf_pair *author = jsmnf_find_path(pairs, json, path, pathSize);
  if (author == NULL) return -1;

  char *author_str = frequenc_safe_malloc(author->v.len + 1);
  frequenc_fast_copy(json + author->v.pos, author_str, author->v.len);

  path[pathLen] = "length";
  jsmnf_pair *length = jsmnf_find_path(pairs, json, path, pathSize);
  if (length == NULL) return -1;

  char *length_str = frequenc_safe_malloc(length->v.len + 1);
  frequenc_fast_copy(json + length->v.pos, length_str, length->v.len);
  long length_num = strtol(length_str, NULL, 10);
  free(length_str);

  path[pathLen] = "identifier";
  jsmnf_pair *identifier = jsmnf_find_path(pairs, json, path, pathSize);
  if (identifier == NULL) return -1;

  char *identifier_str = frequenc_safe_malloc(identifier->v.len + 1);
  frequenc_fast_copy(json + identifier->v.pos, identifier_str, identifier->v.len);

  path[pathLen] = "isStream";
  jsmnf_pair *isStream = jsmnf_find_path(pairs, json, path, pathSize);
  if (isStream == NULL) return -1;
  bool isStream_bool = json[isStream->v.pos] == 't';

  path[pathLen] = "uri";
  jsmnf_pair *uri = jsmnf_find_path(pairs, json, path, pathSize);

  char *uri_str = frequenc_safe_malloc(uri->v.len + 1);
  if (uri_str == NULL) return -1;
  frequenc_fast_copy(json + uri->v.pos, uri_str, uri->v.len);

  path[pathLen] = "artworkUrl";
  jsmnf_pair *artworkUrl = jsmnf_find_path(pairs, json, path, pathSize);

  char *artworkUrl_str = NULL;
  if (artworkUrl != NULL) {
    artworkUrl_str = frequenc_safe_malloc(artworkUrl->v.len + 1);
    frequenc_fast_copy(json + artworkUrl->v.pos, artworkUrl_str, artworkUrl->v.len);
  }

  path[pathLen] = "isrc";
  jsmnf_pair *isrc = jsmnf_find_path(pairs, json, path, pathSize);

  char *isrc_str = NULL;
  if (isrc != NULL) {
    isrc_str = frequenc_safe_malloc(isrc->v.len + 1);
    frequenc_fast_copy(json + isrc->v.pos, isrc_str, isrc->v.len);
  }

  path[pathLen] = "sourceName";
  jsmnf_pair *sourceName = jsmnf_find_path(pairs, json, path, pathSize);
  if (sourceName == NULL) return -1;

  char *sourceName_str = frequenc_safe_malloc(sourceName->v.len + 1);
  frequenc_fast_copy(json + sourceName->v.pos, sourceName_str, sourceName->v.len);

  *track_info = (struct frequenc_track_info) {
    .title = title_str,
    .author = author_str,
    .length = length_num,
    .identifier = identifier_str,
    .isStream = isStream_bool,
    .uri = uri_str,
    .artworkUrl = artworkUrl_str,
    .isrc = isrc_str,
    .sourceName = sourceName_str
  };

  return 0;
}

void frequenc_free_track_info(struct frequenc_track_info *track_info) {
  frequenc_cleanup(track_info->title);
  frequenc_cleanup(track_info->author);
  frequenc_cleanup(track_info->identifier);
  frequenc_cleanup(track_info->uri);
  frequenc_cleanup(track_info->artworkUrl);
  frequenc_cleanup(track_info->isrc);
  frequenc_cleanup(track_info->sourceName);
}

void frequenc_stringify_int(int length, char *result, size_t result_size) {
  snprintf(result, result_size, "%d", length);
}

int frequenc_parse_client_info(char *client_info, struct frequenc_client_info *result) {
  struct tstr_string_token separation_result;
  tstr_find_between(&separation_result, client_info, " ", 0, " ", 0);

  if (separation_result.start == 0) return -1;

  struct tstr_string_token name_result;
  tstr_find_between(&name_result, client_info, NULL, 0, "/", separation_result.start);

  if (name_result.end == 0) return -1;

  struct tstr_string_token version_result;
  tstr_find_between(&version_result, client_info, "/", name_result.end, " ", 0);

  if (version_result.start == 0 || version_result.end == 0) return -1;

  struct tstr_string_token bot_name_result;
  tstr_find_between(&bot_name_result, client_info, "(", separation_result.end, ")", 0);

  if (bot_name_result.start == 0 || bot_name_result.end == 0) return -1;

  result->name = frequenc_safe_malloc(name_result.end - name_result.start + 1);
  frequenc_fast_copy(client_info + name_result.start, result->name, name_result.end - name_result.start);

  result->version = frequenc_safe_malloc(version_result.end - version_result.start + 1);
  frequenc_fast_copy(client_info + version_result.start, result->version, version_result.end - version_result.start);

  result->bot_name = frequenc_safe_malloc(bot_name_result.end - bot_name_result.start + 1);
  frequenc_fast_copy(client_info + bot_name_result.start, result->bot_name, bot_name_result.end - bot_name_result.start);

  return 0;
}

void frequenc_free_client_info(struct frequenc_client_info *client) {
  frequenc_cleanup(client->name);
  frequenc_cleanup(client->version);
  frequenc_cleanup(client->bot_name);
}

void frequenc_sleep(int ms) {
  #ifdef _WIN32
    Sleep(ms);
  #else
    #if _POSIX_C_SOURCE >= 199309L
      struct timespec ts;
      ts.tv_sec = ms / 1000;
      ts.tv_nsec = (ms % 1000) * 1000000;

      nanosleep(&ts, NULL);
    #else
      usleep(ms * 1000);
    #endif
  #endif
}
