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

void frequenc_safe_free(void *pointer) {
  if (pointer != NULL) {
    free(pointer);

    pointer = NULL;
  }
}

void frequenc_fast_copy(const char *src, char *dest, size_t size) {
  memcpy(dest, src, size);
  dest[size] = '\0';
}

char *frequenc_strdup(const char *str, size_t size) {
  size_t len = size == 0 ? strlen(str) : size;
  char *new_str = frequenc_safe_malloc(len + 1);
  frequenc_fast_copy(str, new_str, len);

  return new_str;
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
  frequenc_safe_free(client->name);
  frequenc_safe_free(client->version);
  frequenc_safe_free(client->bot_name);
}
