#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if __linux__
  #include <sys/random.h>
#elif ALLOW_UNSECURE_RANDOM && !_WIN32
  #include <time.h>
#endif
#define JSMN_HEADER
#define JSMN_STRICT /* Set strict mode for jsmn (JSON tokenizer) */
#include "jsmn.h"
#include "jsmn-find.h"

#include "track.h"
#include "types.h"

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

void frequenc_fast_copy(char *src, char *dest, size_t size) {
  memcpy(dest, src, size);
  dest[size] = '\0';
}

void frequenc_cleanup(void *pointer) {
  if (pointer) {
    free(pointer);
    pointer = NULL;
  }
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

int frequenc_track_info_to_json(struct frequenc_track_info *trackInfo, char *encoded, char *result, size_t size) {
  return snprintf(result, size,
    "{"
      "\"encoded\":\"%s\","
      "\"info\":{"
        "\"title\":\"%s\","
        "\"author\":\"%s\","
        "\"length\":%ld,"
        "\"identifier\":\"%s\","
        "\"isStream\":%s,"
        "\"uri\":%s%s%s,"
        "\"artworkUrl\":%s%s%s,"
        "\"isrc\":%s%s%s,"
        "\"sourceName\":\"%s\","
        "\"position\":%ld"
      "}"
    "}", encoded, trackInfo->title, trackInfo->author, trackInfo->length, trackInfo->identifier, trackInfo->isStream ? "true" : "false", trackInfo->uri == NULL ? "null" : "\"", trackInfo->uri == NULL ? "" : trackInfo->uri, trackInfo->uri == NULL ? "" : "\"", trackInfo->artworkUrl == NULL ? "null" : "\"", trackInfo->artworkUrl == NULL ? "" : trackInfo->artworkUrl, trackInfo->artworkUrl == NULL ? "" : "\"", trackInfo->isrc == NULL ? "null" : "\"", trackInfo->isrc == NULL ? "" : trackInfo->isrc, trackInfo->isrc == NULL ? "" : "\"", trackInfo->sourceName, trackInfo->position
  );
}

int frequenc_partial_track_info_to_json(struct frequenc_track_info *trackInfo, char *result, size_t size) {
  return snprintf(result, size,
    "{"
      "\"title\":\"%s\","
      "\"author\":\"%s\","
      "\"length\":%ld,"
      "\"identifier\":\"%s\","
      "\"isStream\":%s,"
      "\"uri\":%s%s%s,"
      "\"artworkUrl\":%s%s%s,"
      "\"isrc\":%s%s%s,"
      "\"sourceName\":\"%s\","
      "\"position\":%ld"
    "}", trackInfo->title, trackInfo->author, trackInfo->length, trackInfo->identifier, trackInfo->isStream ? "true" : "false", trackInfo->uri == NULL ? "null" : "\"", trackInfo->uri == NULL ? "" : trackInfo->uri, trackInfo->uri == NULL ? "" : "\"", trackInfo->artworkUrl == NULL ? "null" : "\"", trackInfo->artworkUrl == NULL ? "" : trackInfo->artworkUrl, trackInfo->artworkUrl == NULL ? "" : "\"", trackInfo->isrc == NULL ? "null" : "\"", trackInfo->isrc == NULL ? "" : trackInfo->isrc, trackInfo->isrc == NULL ? "" : "\"", trackInfo->sourceName, trackInfo->position
  );
}

int frequenc_json_to_track_info(struct frequenc_track_info *trackInfo, jsmnf_pair *pairs, char *json, char *path[], int pathLen, int pathSize) {
  path[pathLen] = "title";
  jsmnf_pair *title = jsmnf_find_path(pairs, json, path, pathSize);
  if (title == NULL) return -1;

  char *titleStr = frequenc_safe_malloc(title->v.len + 1);
  frequenc_fast_copy(json + title->v.pos, titleStr, title->v.len);

  path[pathLen] = "author";
  jsmnf_pair *author = jsmnf_find_path(pairs, json, path, pathSize);
  if (author == NULL) return -1;

  char *authorStr = frequenc_safe_malloc(author->v.len + 1);
  frequenc_fast_copy(json + author->v.pos, authorStr, author->v.len);

  path[pathLen] = "length";
  jsmnf_pair *length = jsmnf_find_path(pairs, json, path, pathSize);
  if (length == NULL) return -1;

  char *lengthStr = frequenc_safe_malloc(length->v.len + 1);
  frequenc_fast_copy(json + length->v.pos, lengthStr, length->v.len);
  long lengthNum = strtol(lengthStr, NULL, 10);
  free(lengthStr);

  path[pathLen] = "identifier";
  jsmnf_pair *identifier = jsmnf_find_path(pairs, json, path, pathSize);
  if (identifier == NULL) return -1;

  char *identifierStr = frequenc_safe_malloc(identifier->v.len + 1);
  frequenc_fast_copy(json + identifier->v.pos, identifierStr, identifier->v.len);

  path[pathLen] = "isStream";
  jsmnf_pair *isStream = jsmnf_find_path(pairs, json, path, pathSize);
  if (isStream == NULL) return -1;
  bool isStreamBool = json[isStream->v.pos] == 't';

  path[pathLen] = "uri";
  jsmnf_pair *uri = jsmnf_find_path(pairs, json, path, pathSize);

  char *uriStr = frequenc_safe_malloc(uri->v.len + 1);
  frequenc_fast_copy(json + uri->v.pos, uriStr, uri->v.len);

  path[pathLen] = "artworkUrl";
  jsmnf_pair *artworkUrl = jsmnf_find_path(pairs, json, path, pathSize);

  char *artworkUrlStr = frequenc_safe_malloc(artworkUrl->v.len + 1);
  frequenc_fast_copy(json + artworkUrl->v.pos, artworkUrlStr, artworkUrl->v.len);

  path[pathLen] = "isrc";
  jsmnf_pair *isrc = jsmnf_find_path(pairs, json, path, pathSize);

  char *isrcStr = frequenc_safe_malloc(isrc->v.len + 1);
  frequenc_fast_copy(json + isrc->v.pos, isrcStr, isrc->v.len);

  path[pathLen] = "sourceName";
  jsmnf_pair *sourceName = jsmnf_find_path(pairs, json, path, pathSize);
  if (sourceName == NULL) return -1;

  char *sourceNameStr = frequenc_safe_malloc(sourceName->v.len + 1);
  frequenc_fast_copy(json + sourceName->v.pos, sourceNameStr, sourceName->v.len);

  path[pathLen] = "position";
  jsmnf_pair *position = jsmnf_find_path(pairs, json, path, pathSize);
  if (position == NULL) return -1;

  char *positionStr = frequenc_safe_malloc(position->v.len + 1);
  frequenc_fast_copy(json + position->v.pos, positionStr, position->v.len);
  long positionNum = strtol(positionStr, NULL, 10);
  free(positionStr);

  *trackInfo = (struct frequenc_track_info) {
    .title = titleStr,
    .author = authorStr,
    .length = lengthNum,
    .identifier = identifierStr,
    .isStream = isStreamBool,
    .uri = strcmp(uriStr, "null") == 0 ? NULL : uriStr,
    .artworkUrl = strcmp(artworkUrlStr, "null") == 0 ? NULL : artworkUrlStr,
    .isrc = strcmp(isrcStr, "null") == 0 ? NULL : isrcStr,
    .sourceName = sourceNameStr,
    .position = positionNum
  };

  if (strcmp(uriStr, "null") == 0) frequenc_cleanup(uriStr);
  if (strcmp(artworkUrlStr, "null") == 0) frequenc_cleanup(artworkUrlStr);
  if (strcmp(isrcStr, "null") == 0) frequenc_cleanup(isrcStr);

  return 0;
}

void frequenc_free_track_info(struct frequenc_track_info *trackInfo) {
  frequenc_cleanup(trackInfo->title);
  frequenc_cleanup(trackInfo->author);
  frequenc_cleanup(trackInfo->identifier);
  frequenc_cleanup(trackInfo->uri);
  frequenc_cleanup(trackInfo->artworkUrl);
  frequenc_cleanup(trackInfo->isrc);
  frequenc_cleanup(trackInfo->sourceName);
}

void frequenc_stringify_int(int length, char *result) {
  snprintf(result, sizeof(result), "%d", length);
}
