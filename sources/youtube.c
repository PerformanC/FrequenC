#include <stdio.h>
#include <string.h>

#include "libtstr.h"
#include "pjsonb.h"

#include "utils.h"
#include "httpclient.h"
#include "sources.h"

#include "youtube.h"

#define _frequenc_youtube_get_client_name(type) (type == 0 ? "ANDROID" : "ANDROID_MUSIC")
#define _frequenc_youtube_get_client_version(type) (type == 0 ? "19.13.34" : "6.42.52")

/* TODO: Replace for a macro for using stack allocated chars */
void frequenc_youtube_search(struct tstr_string *result, char *query, int type) {
  /* todo: add YouTube music support based on NodeLink */
  (void)type;

  struct pjsonb body_json;
  pjsonb_init(&body_json, PJSONB_OBJECT);

  pjsonb_enter_object(&body_json, "context");

  pjsonb_enter_object(&body_json, "client");

  pjsonb_set_string(&body_json, "userAgent", "com.google.android.youtube/19.13.34 (Linux; U; Android 14 gzip)", sizeof("com.google.android.youtube/19.13.34 (Linux; U; Android 14 gzip)") - 1);
  pjsonb_set_string(&body_json, "clientName", "ANDROID", sizeof("ANDROID") - 1);
  pjsonb_set_string(&body_json, "clientVersion", "19.13.34", sizeof("19.13.34") - 1);
  pjsonb_set_int(&body_json, "screenDensityFloat", 1);
  pjsonb_set_int(&body_json, "screenHeightPoints", 1080);
  pjsonb_set_int(&body_json, "screenPixelDensity", 1);
  pjsonb_set_int(&body_json, "screenWidthPoints", 1920);

  pjsonb_leave_object(&body_json);

  pjsonb_leave_object(&body_json);

  pjsonb_set_string(&body_json, "query", query, strlen(query));
  pjsonb_set_string(&body_json, "params", "EgIQAQ%%3D%%3D", sizeof("EgIQAQ%%3D%%3D") - 1);

  pjsonb_end(&body_json);

  struct httpclient_request_params request = {
    .host = "www.youtube.com",
    .port = 443,
    .secure = true,
    .method = "POST",
    .path = "/youtubei/v1/search?key=AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8&prettyPrint=false",
    .headers_length = 2,
    .headers = (struct httpparser_header[]) {
      {
        .key = "User-Agent",
        .value = "com.google.android.youtube/19.13.34 (Linux; U; Android 34 gzip)"
      },
      {
        .key = "Content-Type",
        .value = "application/json"
      }
    },
    .body = &(struct tstr_string) {
      .string = body_json.string,
      .length = (size_t)body_json.position,
      .allocated = true
    }
  };

  struct httpclient_response response;
  if (httpclient_request(&request, &response) == -1) {
    /* Should be printed by httpclient
       printf("[youtube]: Failed to make request.\n"); */

    pjsonb_free(&body_json);

    result->string = "{"
                       "\"loadType\":\"error\","
                       "\"data\":\"Failed to connect to YouTube.\""
                     "}";
    result->length = 59;
    result->allocated = false;

    return;
  }

  httpclient_shutdown(&response);
  pjsonb_free(&body_json);

  jsmn_parser parser;
  jsmntok_t *tokens = NULL;
  unsigned num_tokens = 0;

  jsmn_init(&parser);
  int r = jsmn_parse_auto(&parser, response.body, response.body_length, &tokens, &num_tokens);
  if (r <= 0) {
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[youtube]: Failed to parse JSON: %d\n", r);

    result->string = "{"
                       "\"loadType\":\"error\","
                       "\"data\":\"Failed to parse the JSON.\""
                       "}";
    result->length = 55;
    result->allocated = false;

    return;
  }

  jsmnf_loader loader;
  jsmnf_pair *pairs = NULL;
  unsigned num_pairs = 0;

  jsmnf_init(&loader);
  r = jsmnf_load_auto(&loader, response.body, tokens, num_tokens, &pairs, &num_pairs);
  if (r <= 0) {
    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[youtube]: Error while loading tokens: %d\n", r);

    result->string = "{"
                       "\"loadType\":\"error\","
                       "\"data\":\"Failed to load the JSON tokens.\""
                     "}";
    result->length = 61;
    result->allocated = false;

    return;
  }

  char *path[20] = { "error", "message" };
  jsmnf_pair *error = jsmnf_find_path(pairs, response.body, path, 1);

  if (error != NULL) {
    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[youtube]: Error: %.*s\n", (int)error->v.len, response.body + error->v.pos);

    char *error_json = malloc(((30 + error->v.len) + 1) * sizeof(char));
    int error_json_length = snprintf(error_json, ((30 + error->v.len) + 1) * sizeof(char),
      "{"
        "\"loadType\":\"error\","
        "\"data\":\"%.*s\""
      "}", (int)error->v.len, response.body + error->v.pos);

    result->string = error_json;
    result->length = (size_t)error_json_length;
    result->allocated = true;

    return;
  }

  path[0] = "contents";
  path[1] = "sectionListRenderer";
  path[2] = "contents";

  jsmnf_pair *contents = jsmnf_find_path(pairs, response.body, path, 3);

  char i_str[11 + 1];
  snprintf(i_str, sizeof(i_str), "%d", contents->size - 1);

  path[3] = i_str;
  path[4] = "itemSectionRenderer";
  path[5] = "contents";

  jsmnf_pair *videos = jsmnf_find_path(pairs, response.body, path, 6);

  struct pjsonb track_json;
  pjsonb_init(&track_json, PJSONB_OBJECT);

  pjsonb_set_string(&track_json, "loadType", "search", sizeof("search") - 1);

  pjsonb_enter_array(&track_json, "data");

  int i = 0;
  while (i < videos->size) {
    char i2_str[11 + 1];
    snprintf(i2_str, sizeof(i2_str), "%d", i);

    path[6] = i2_str;

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

      path[8] = "thumbnail";
      path[9] = "thumbnails";
      jsmnf_pair *thumbnails = jsmnf_find_path(pairs, response.body, path, 10);
      
      char i3_str[11 + 1];
      snprintf(i3_str, sizeof(i3_str), "%d", thumbnails->size - 1);

      path[10] = i3_str;
      path[11] = "url";
      jsmnf_pair *artwork_url = jsmnf_find_path(pairs, response.body, path, 12);

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
            length_int += (size_t)atoi(length_str) * 1000;

            break;
          }
          case 1: {
            struct tstr_string_token length_str_utils;
            tstr_find_between(&length_str_utils, length_str, NULL, 0, ":", 0);

            char *length_str_minutes = frequenc_safe_malloc(((size_t)length_str_utils.end + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils.start, length_str_minutes, (size_t)length_str_utils.end);

            length_int += (size_t)atoi(length_str_minutes) * (60 * 1000);

            frequenc_unsafe_free(length_str_minutes);

            char *length_str_seconds = frequenc_safe_malloc((length->v.len - (size_t)length_str_utils.end + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils.end + 1, length_str_seconds, length->v.len - (size_t)length_str_utils.end);

            length_int += (size_t)atoi(length_str_seconds) * 1000;

            frequenc_unsafe_free(length_str_seconds);

            break;
          }
          case 2: {
            struct tstr_string_token length_str_utils;
            tstr_find_between(&length_str_utils, length_str, NULL, 0, ":", 0);

            char *length_str_hours = frequenc_safe_malloc((size_t)(length_str_utils.end + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils.start, length_str_hours, (size_t)length_str_utils.end);

            length_int += (size_t)atoi(length_str_hours) * (60 * 60 * 1000);

            frequenc_unsafe_free(length_str_hours);

            struct tstr_string_token length_str_utils2;
            tstr_find_between(&length_str_utils2, length_str, NULL, length_str_utils.end + 1, ":", 0);

            char *length_str_minutes = frequenc_safe_malloc((size_t)(length_str_utils2.end - length_str_utils2.start + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils2.start, length_str_minutes, (size_t)(length_str_utils2.end - length_str_utils2.start));

            length_int += (size_t)atoi(length_str_minutes) * (60 * 1000);

            frequenc_unsafe_free(length_str_minutes);

            char *length_str_seconds = frequenc_safe_malloc((length->v.len - (size_t)length_str_utils2.end + 1) * sizeof(char));
            frequenc_fast_copy(length_str + length_str_utils2.end + 1, length_str_seconds, length->v.len - (size_t)length_str_utils2.end);

            length_int += (size_t)atoi(length_str_seconds) * 1000;

            frequenc_unsafe_free(length_str_seconds);

            break;
          }
        }
      }

      frequenc_unsafe_free(length_str);

      char *title_str = frequenc_safe_malloc((title->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + title->v.pos, title_str, title->v.len);

      char *uri_str = frequenc_safe_malloc(((sizeof("https://www.youtube.com/watch?v=") - 1) + identifier->v.len + 1) * sizeof(char));
      size_t uri_len = (size_t)snprintf(uri_str, ((sizeof("https://www.youtube.com/watch?v=") - 1) + identifier->v.len + 1) * sizeof(char), "https://www.youtube.com/watch?v=%s", identifier_str);

      char *artwork_url_str = frequenc_safe_malloc((artwork_url->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + artwork_url->v.pos, artwork_url_str, artwork_url->v.len);

      struct frequenc_track_info track_info = {
        .title = (struct tstr_string) {
          .string = title_str,
          .length = title->v.len,
          .allocated = true
        },
        .author = (struct tstr_string) {
          .string = author_str,
          .length = author->v.len,
          .allocated = true
        },
        .length = length_int,
        .identifier = (struct tstr_string) {
          .string = identifier_str,
          .length = identifier->v.len,
          .allocated = true
        },
        .is_stream = !length_int,
        .uri = (struct tstr_string) {
          .string = uri_str,
          .length = uri_len,
          .allocated = true
        },
        .artwork_url = (struct tstr_string) {
          .string = artwork_url_str,
          .length = artwork_url->v.len,
          .allocated = true
        },
        .isrc = (struct tstr_string) {
          .string = NULL,
          .length = 0,
          .allocated = false
        },
        .source_name = (struct tstr_string) {
          .string = "YouTube",
          .length = sizeof("YouTube") - 1,
          .allocated = false
        },
      };

      struct tstr_string encoded_track = { 0 };
      frequenc_encode_track(&track_info, &encoded_track);

      frequenc_track_info_to_json(&track_info, &encoded_track, &track_json, false);

      frequenc_free_track_info(&track_info);
      tstr_free(&encoded_track);
    }

    i++;
  }

  if (track_json.position == 29) {
    pjsonb_free(&track_json);
    frequenc_unsafe_free(response.body);
    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    result->string = "{"
                       "\"loadType\":\"empty\""
                     "}";
    result->length = 20;
    result->allocated = false;

    return;
  }

  pjsonb_leave_array(&track_json);

  pjsonb_end(&track_json);

  frequenc_unsafe_free(pairs);
  frequenc_unsafe_free(tokens);
  httpclient_free(&response);

  result->string = track_json.string;
  result->length = (size_t)track_json.position;
  result->allocated = true;

  return;
}

int frequenc_youtube_get_stream(struct frequenc_audio_stream *stream, struct frequenc_track_info track_info) {
  struct pjsonb body_json;
  pjsonb_init(&body_json, PJSONB_OBJECT);

  pjsonb_enter_object(&body_json, "context");

  pjsonb_enter_object(&body_json, "client");

  pjsonb_set_string(&body_json, "userAgent", "com.google.android.youtube/19.13.34 (Linux; U; Android 14 gzip)", sizeof("com.google.android.youtube/19.13.34 (Linux; U; Android 14 gzip)") - 1);
  pjsonb_set_string(&body_json, "clientName", "ANDROID", sizeof("ANDROID") - 1);
  pjsonb_set_string(&body_json, "clientVersion", "19.13.34", sizeof("19.13.34") - 1);
  pjsonb_set_int(&body_json, "screenDensityFloat", 1);
  pjsonb_set_int(&body_json, "screenHeightPoints", 1080);
  pjsonb_set_int(&body_json, "screenPixelDensity", 1);
  pjsonb_set_int(&body_json, "screenWidthPoints", 1920);

  pjsonb_leave_object(&body_json);

  pjsonb_leave_object(&body_json);

  pjsonb_set_string(&body_json, "videoId", track_info.identifier.string, track_info.identifier.length);
  pjsonb_set_bool(&body_json, "contentCheckOk", true);
  pjsonb_set_bool(&body_json, "racyCheckOk", true);
  pjsonb_set_string(&body_json, "params", "CgIQBg", sizeof("CgIQBg") - 1);

  pjsonb_end(&body_json);

  struct httpclient_request_params request = {
    .host = "www.youtube.com",
    .port = 443,
    .secure = true,
    .method = "POST",
    .path = "/youtubei/v1/player",
    .headers_length = 2,
    .headers = (struct httpparser_header[]) {
      {
        .key = "User-Agent",
        .value = "com.google.android.youtube/19.13.34 (Linux; U; Android 34 gzip)"
      },
      {
        .key = "Content-Type",
        .value = "application/json"
      }
    },
    .body = &(struct tstr_string) {
      .string = body_json.string,
      .length = (size_t)body_json.position,
      .allocated = true
    }
  };

  struct httpclient_response response;
  if (httpclient_request(&request, &response) == -1) {
    /* Should be printed by httpclient
       printf("[youtube]: Failed to make request.\n"); */

    pjsonb_free(&body_json);

    return -1;
  }

  httpclient_shutdown(&response);
  pjsonb_free(&body_json);

  if (response.status != 200) {
    httpclient_free(&response);

    printf("%d\n", response.status);

    printf("[youtube]: Failed to get YouTube stream, received an unexpected status code from the HTTP request to YouTube. Expected 200, received %d.\n", response.status);

    return -1;
  }

  jsmn_parser parser;
  jsmntok_t *tokens = NULL;
  unsigned num_tokens = 0;

  jsmn_init(&parser);
  int r = jsmn_parse_auto(&parser, response.body, response.body_length, &tokens, &num_tokens);
  if (r <= 0) {
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[youtube]: Failed to parse JSON: %d\n", r);

    return -1;
  }

  jsmnf_loader loader;
  jsmnf_pair *pairs = NULL;
  unsigned num_pairs = 0;

  jsmnf_init(&loader);
  r = jsmnf_load_auto(&loader, response.body, tokens, num_tokens, &pairs, &num_pairs);
  if (r <= 0) {
    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[youtube]: Error while loading tokens: %d\n", r);

    return -1;
  }

  char *path[4] = { "error", "message" };
  jsmnf_pair *error_message = jsmnf_find_path(pairs, response.body, path, 1);

  if (error_message != NULL) {
    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[frequenc:youtube]: Error while retrieving the YouTube track stream: %.*s\n", (int)error_message->v.len, response.body + error_message->v.pos);

    return -1;
  }

  path[0] = "playabilityStatus";
  path[1] = "status";
  jsmnf_pair *status = jsmnf_find_path(pairs, response.body, path, 2);

  if (status == NULL) {
    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    printf("[frequenc:youtube]: Failed to find the playability status in the YouTube response.\n");

    return -1;
  }

  if (strncmp(response.body + status->v.pos, "OK", status->v.len) != 0) {
    path[1] = "reason";

    jsmnf_pair *reason = jsmnf_find_path(pairs, response.body, path, 2);

    printf("[frequenc:youtube]: The YouTube track is not available for playback. Reason: %.*s\n", (int)reason->v.len, response.body + reason->v.pos);

    frequenc_unsafe_free(pairs);
    frequenc_unsafe_free(tokens);
    httpclient_free(&response);

    return -1;
  }

  path[0] = "streamingData";
  path[1] = "adaptiveFormats";

  jsmnf_pair *adaptive_formats = jsmnf_find_path(pairs, response.body, path, 2);

  for (unsigned short i = 0; i < adaptive_formats->size; i++) {
    char i_str[8 + 1];
    snprintf(i_str, sizeof(i_str), "%d", i);

    path[2] = i_str;

    path[3] = "itag";
    jsmnf_pair *itag_pair = jsmnf_find_path(pairs, response.body, path, 4);

    if (itag_pair == NULL) {
      frequenc_unsafe_free(pairs);
      frequenc_unsafe_free(tokens);
      httpclient_free(&response);

      printf("[frequenc:youtube]: Failed to find the itag in the YouTube response.\n");

      return -1;
    }

    char itag_str[4 + 1];
    frequenc_fast_copy(response.body + itag_pair->v.pos, itag_str, itag_pair->v.len);

    int itag = atoi(itag_str);

    if (itag != FREQUENC_YOUTUBE_AUDIO_ITAG && i == adaptive_formats->size - 1) {
      frequenc_unsafe_free(pairs);
      frequenc_unsafe_free(tokens);
      httpclient_free(&response);

      printf("[frequenc:youtube]: Failed to find the YouTube audio stream in the adaptive formats.\n");

      return -1;
    }

    if (itag != FREQUENC_YOUTUBE_AUDIO_ITAG) continue;

    path[3] = "url";
    jsmnf_pair *url_pair = jsmnf_find_path(pairs, response.body, path, 4);

    if (url_pair == NULL) {
      frequenc_unsafe_free(pairs);
      frequenc_unsafe_free(tokens);
      httpclient_free(&response);

      printf("[frequenc:youtube]: Failed to find the URL in the YouTube response.\n");

      return -1;
    }

    path[3] = "mimeType";
    jsmnf_pair *mime_type = jsmnf_find_path(pairs, response.body, path, 4);

    if (mime_type == NULL) {
      frequenc_unsafe_free(pairs);
      frequenc_unsafe_free(tokens);
      httpclient_free(&response);

      printf("[frequenc:youtube]: Failed to find the mime type in the YouTube response.\n");

      return -1;
    }

    if (strncmp(response.body + mime_type->v.pos, "audio/webm; codecs=\\\"opus\\\"", mime_type->v.len) != 0) {
      frequenc_unsafe_free(pairs);
      frequenc_unsafe_free(tokens);
      httpclient_free(&response);

      printf("[frequenc:youtube]: The YouTube audio track outputted an unsuported audio type.\n");

      return -1;
    }

    stream->url.string = frequenc_safe_malloc(url_pair->v.len + (sizeof("&rn=1&ratebypass=yes&range=0-") - 1));
    stream->url.length = url_pair->v.len;
    frequenc_unsafe_fast_copy(response.body + url_pair->v.pos, stream->url.string, url_pair->v.len);
    frequenc_unsafe_fast_copy("&rn=1&ratebypass=yes&range=0-", stream->url.string + url_pair->v.len, sizeof("&rn=1&ratebypass=yes&range=0-") - 1);

    stream->type = FREQUENC_AUDIO_STREAM_TYPE_WEBM_OPUS;

    break;
  }

  frequenc_unsafe_free(pairs);
  frequenc_unsafe_free(tokens);
  httpclient_free(&response);

  return 0;
}
