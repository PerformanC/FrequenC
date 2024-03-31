#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "libtstr.h"
#include "httpclient.h"

#define _frequenc_youtube_get_client_name(type) (type == 0 ? "ANDROID" : "ANDROID_MUSIC")
#define _frequenc_youtube_get_client_version(type) (type == 0 ? "19.10.33" : "6.42.52")

/* TODO: Replace for a macro for using stack allocated chars */
struct tstr_string frequenc_youtube_search(char *query, int type) {
  /* todo: add YouTube music support based on NodeLink */
  (void)type;

  size_t body_size = (271 + strlen(_frequenc_youtube_get_client_name(type)) + strlen(_frequenc_youtube_get_client_version(type)) + strlen(query) + 1) * sizeof(char);
  char *body = frequenc_safe_malloc(body_size);
  snprintf(body, body_size,
  "{"
    "\"context\":{"
      "\"thirdParty\":{"
        "\"embedUrl\":\"https://google.com\""
      "},"
      "\"client\":{"
        "\"clientName\":\"%s\","
        "\"clientVersion\":\"%s\","
        "\"androidSdkVersion\":\"34\","
        "\"screenDensityFloat\":1,"
        "\"screenHeightPoints\":1080,"
        "\"screenPixelDensity\":1,"
        "\"screenWidthPoints\":1920"
      "}"
    "},"
    "\"query\":\"%s\","
    "\"params\":\"EgIQAQ%%3D%%3D\"" // %% = escaped %
  "}", _frequenc_youtube_get_client_name(type), _frequenc_youtube_get_client_version(type), query);

  struct httpclient_request_params request = {
    .host = "www.youtube.com",
    .port = 443,
    .method = "POST",
    .path = "/youtubei/v1/search?key=AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8&prettyPrint=false",
    .headers_length = 2,
    .headers = (struct httpparser_header[]) {
      {
        .key = "User-Agent",
        .value = "com.google.android.youtube/17.29.34 (Linux; U; Android 34 gzip)"
      },
      {
        .key = "Content-Type",
        .value = "application/json"
      }
    },
    .body = body
  };

  struct httpclient_response response;
  if (httpclient_request(&request, &response) == -1) {
    /* Should be printed by httpclient
       printf("[youtube]: Failed to make request.\n"); */

    free(body);

    char *error = frequenc_safe_malloc((60 + 1) * sizeof(char));
    snprintf(error, (60 + 1) * sizeof(char),
      "{"
        "\"loadType\":\"error\","
        "\"data\":\"Failed to connect to YouTube.\""
      "}");

    struct tstr_string result = {
      .string = error,
      .length = 60,
      .allocated = true
    };

    return result;
  }

  httpclient_shutdown(&response);

  jsmn_parser parser;
  jsmntok_t *tokens = NULL;
  unsigned num_tokens = 0;

  jsmn_init(&parser);
  int r = jsmn_parse_auto(&parser, response.body, response.body_length, &tokens, &num_tokens);
  if (r <= 0) {
    free(tokens);
    free(body);

    printf("[youtube]: Failed to parse JSON: %d\n", r);

    char *error = frequenc_safe_malloc((55 + 1) * sizeof(char));
    snprintf(error, (55 + 1) * sizeof(char),
      "{"
        "\"loadType\":\"error\","
        "\"data\":\"Failed to parse the JSON.\""
      "}");

    struct tstr_string result = {
      .string = error,
      .length = 55,
      .allocated = true
    };

    return result;
  }

  jsmnf_loader loader;
  jsmnf_pair *pairs = NULL;
  unsigned num_pairs = 0;

  jsmnf_init(&loader);
  r = jsmnf_load_auto(&loader, response.body, tokens, num_tokens, &pairs, &num_pairs);
  if (r <= 0) {
    free(pairs);
    free(tokens);
    free(body);

    printf("[youtube]: Error while loading tokens: %d\n", r);

    char *error = frequenc_safe_malloc((61 + 1) * sizeof(char));
    snprintf(error, (61 + 1) * sizeof(char),
      "{"
        "\"loadType\":\"error\","
        "\"data\":\"Failed to load the JSON tokens.\""
      "}");

    struct tstr_string result = {
      .string = error,
      .length = 61,
      .allocated = true
    };

    return result;
  }

  char *path[20] = { "error", "message" };
  jsmnf_pair *error = jsmnf_find_path(pairs, response.body, path, 1);

  if (error != NULL) {
    free(pairs);
    free(tokens);
    free(body);

    printf("[youtube]: Error: %.*s\n", (int)error->v.len, response.body + error->v.pos);

    char *error_json = malloc(((30 + error->v.len) + 1) * sizeof(char));
    int error_json_length = snprintf(error_json, ((30 + error->v.len) + 1) * sizeof(char),
      "{"
        "\"loadType\":\"error\","
        "\"data\":\"%.*s\""
      "}", (int)error->v.len, response.body + error->v.pos);

    struct tstr_string result = {
      .string = error_json,
      .length = error_json_length,
      .allocated = true
    };

    return result;
  }

  path[0] = "contents";
  path[1] = "sectionListRenderer";
  path[2] = "contents";

  jsmnf_pair *contents = jsmnf_find_path(pairs, response.body, path, 3);

  char i_str[11 + 1];
  snprintf(i_str, sizeof(i_str), "%d", contents->size - 1); /* TODO: Replace snprintf */

  path[3] = i_str;
  path[4] = "itemSectionRenderer";
  path[5] = "contents";

  jsmnf_pair *videos = jsmnf_find_path(pairs, response.body, path, 6);

  char *response_str = frequenc_safe_malloc((29 + 1) * sizeof(char));
  size_t response_str_length = snprintf(response_str, (29 + 1) * sizeof(char),
    "{"
      "\"loadType\":\"search\","
      "\"data\":[");

  int i = 0;
  while (i < videos->size) {
    char i2Str[11 + 1];
    snprintf(i2Str, sizeof(i2Str), "%d", i); /* TODO: Replace snprintf */

    path[6] = i2Str;

    path[7] = "compactVideoRenderer";
    jsmnf_pair *video = jsmnf_find_path(pairs, response.body, path, 8);

    if (video != NULL) {
      path[8] = "videoId";
      jsmnf_pair *identifier = jsmnf_find_path(pairs, response.body, path, 9);

      path[8] = "longBylineText";
      path[9] = "runs";
      path[10] = "0";
      path[11] = "text";
      jsmnf_pair *author = jsmnf_find_path(pairs, response.body, path, 12);

      path[8] = "lengthText";
      path[9] = "runs";
      path[10] = "0";
      path[11] = "text";
      jsmnf_pair *length = jsmnf_find_path(pairs, response.body, path, 12);

      path[8] = "title";
      path[9] = "runs";
      path[10] = "0";
      path[11] = "text";
      jsmnf_pair *title = jsmnf_find_path(pairs, response.body, path, 12);

      char *identifier_str = frequenc_safe_malloc((identifier->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + identifier->v.pos, identifier_str, identifier->v.len);

      char *author_str = frequenc_safe_malloc((author->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + author->v.pos, author_str, author->v.len);

      char *length_str = frequenc_safe_malloc((length->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + length->v.pos, length_str, length->v.len);

      size_t length_int = 0;

      if (length) {
        int amount = tstr_find_amount(length_str, ":");

        switch (amount) {
          case 0: {
            length_int += atoi(length_str) * 1000;

            break;
          }
          case 1: {
            struct tstr_string_token length_str_utils;
            tstr_find_between(&length_str_utils, length_str, NULL, 0, ":", 0);

            char *length_str_minutes = frequenc_safe_malloc((length_str_utils.end + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils.start, length_str_minutes, length_str_utils.end);

            length_int += atoi(length_str_minutes) * (60 * 1000);

            free(length_str_minutes);

            struct tstr_string_token length_str_utils2;
            tstr_find_between(&length_str_utils2, length_str, NULL, length_str_utils.end + 1, ":", 0);

            char *length_str_seconds = frequenc_safe_malloc((length_str_utils2.end - length_str_utils2.start + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils2.start, length_str_seconds, length_str_utils2.end - length_str_utils2.start);

            length_int += atoi(length_str_seconds) * 1000;

            free(length_str_seconds);

            break;
          }
          case 2: {
            struct tstr_string_token length_str_utils;
            tstr_find_between(&length_str_utils, length_str, NULL, 0, ":", 0);

            char *length_str_hours = frequenc_safe_malloc((length_str_utils.end + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils.start, length_str_hours, length_str_utils.end);

            length_int += atoi(length_str_hours) * (60 * 60 * 1000);

            free(length_str_hours);

            struct tstr_string_token length_str_utils2;
            tstr_find_between(&length_str_utils2, length_str, NULL, length_str_utils.end + 1, ":", 0);

            char *length_str_minutes = frequenc_safe_malloc((length_str_utils2.end - length_str_utils2.start + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils2.start, length_str_minutes, length_str_utils2.end - length_str_utils2.start);

            length_int += atoi(length_str_minutes) * (60 * 1000);

            free(length_str_minutes);

            struct tstr_string_token length_str_utils3;
            tstr_find_between(&length_str_utils3, length_str, NULL, length_str_utils2.end + 1, ":", 0);

            char *length_str_seconds = frequenc_safe_malloc((length_str_utils3.end - length_str_utils3.start + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils3.start, length_str_seconds, length_str_utils3.end - length_str_utils3.start);

            length_int += atoi(length_str_seconds) * 1000;

            free(length_str_seconds);

            break;
          }
        }
      }

      char *title_str = frequenc_safe_malloc((title->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + title->v.pos, title_str, title->v.len);

      char *uri_str = frequenc_safe_malloc(((sizeof("https://www.youtube.com/watch?v=") - 1) + identifier->v.len + 1) * sizeof(char));
      snprintf(uri_str, ((sizeof("https://www.youtube.com/watch?v=") - 1) + identifier->v.len + 1) * sizeof(char), "https://www.youtube.com/watch?v=%s", identifier_str);

      char *artworkUrl_str = frequenc_safe_malloc(((sizeof("https://i.ytimg.com/vi/") - 1) + identifier->v.len + sizeof("/maxresdefault.jpg") - 1 + 1) * sizeof(char));
      snprintf(artworkUrl_str, ((sizeof("https://i.ytimg.com/vi/") - 1) + identifier->v.len + sizeof("/maxresdefault.jpg") - 1 + 1) * sizeof(char), "https://i.ytimg.com/vi/%s/maxresdefault.jpg", identifier_str);

      struct frequenc_track_info trackInfo = {
        .title = title_str,
        .author = author_str,
        .length = length_int,
        .identifier = identifier_str,
        .isStream = length_int == 0,
        .uri = uri_str,
        .artworkUrl = artworkUrl_str,
        .isrc = NULL,
        .sourceName = "YouTube",
        .position = 0
      };

      char *encodedTrack = NULL;
      frequenc_encode_track(&trackInfo, &encodedTrack);

      char video_json[2048];
      frequenc_track_info_to_json(&trackInfo, encodedTrack, video_json, sizeof(video_json));

      if (response_str_length == 29) {
        response_str = realloc(response_str, response_str_length + strlen(video_json) + 2);

        tstr_append(response_str, video_json, &response_str_length, 0);
      } else {
        response_str = realloc(response_str, response_str_length + strlen(video_json) + sizeof(",") - 1 + 2);

        tstr_append(response_str, ",", &response_str_length, 0);
        tstr_append(response_str, video_json, &response_str_length, 0);
      }

      free(encodedTrack);
      free(identifier_str);
      free(author_str);
      free(length_str);
      free(title_str);
      free(uri_str);
      free(artworkUrl_str);
    }

    i++;
  }

  if (response_str_length == 29) {
    free(response_str);
    free(response.body);
    free(pairs);
    free(tokens);
    free(body);

    char *response_msg = frequenc_safe_malloc((20 + 1) * sizeof(char));
    snprintf(response_msg, (20 + 1) * sizeof(char),
      "{"
        "\"loadType\":\"empty\""
      "}");

    struct tstr_string result = {
      .string = response_msg,
      .length = 20,
      .allocated = true
    };

    return result;
  }

  response_str = realloc(response_str, response_str_length + (sizeof("]}") - 1) + 1);
  tstr_append(response_str, "]}", &response_str_length, 0);

  free(pairs);
  free(tokens);
  httpclient_free(&response);
  free(body);

  struct tstr_string result = {
    .string = response_str,
    .length = response_str_length,
    .allocated = true
  };

  return result;
}
