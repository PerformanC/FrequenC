#include <stdio.h>
#include <string.h>
#include "utils.h"
#include "libtstr.h"
#include "httpclient.h"

/* TODO: Replace for a macro for using stack allocated chars */
struct tstr_string frequenc_search_youtube(char *query, int type) {
  (void)type;

  char body[1024];
  snprintf(body, sizeof(body),
  "{"
    "\"context\":{"
      "\"thirdParty\":{"
        "\"embedUrl\":\"https://google.com\""
      "},"
      "\"client\":{"
        "\"clientName\":\"ANDROID\","
        "\"clientVersion\":\"19.04.33\","
        "\"androidSdkVersion\":\"34\","
        "\"screenDensityFloat\":1,"
        "\"screenHeightPoints\":1080,"
        "\"screenPixelDensity\":1,"
        "\"screenWidthPoints\":1920"
      "}"
    "},"
    "\"query\":\"%s\","
    "\"params\":\"EgIQAQ%%3D%%3D\"" // %% = escaped %
  "}", query);

  struct httpclient_request_params request = {
    .host = "www.youtube.com",
    .port = 443,
    .method = "POST",
    .path = "/youtubei/v1/search?key=AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8&prettyPrint=false",
    .headersLength = 2,
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

    char *error = frequenc_safe_malloc((60 + 1) * sizeof(char));
    snprintf(error, 60 + 1,
      "{"
        "\"loadType\":\"error\","
        "\"error\":\"Failed to connect to YouTube.\""
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
  jsmntok_t *tokens = frequenc_safe_malloc((1024 * 1024 * 1) * sizeof(jsmntok_t)); /* TODO: Replace by auto load */

  jsmn_init(&parser);
  int r = jsmn_parse(&parser, response.body, response.bodyLength, tokens, 1024 * 1024 * 1);
  if (r < 0) {
    printf("[youtube]: Failed to parse JSON.\n");

    char error[] =
      "{"
        "\"loadType\":\"error\","
        "\"error\":\"Failed to parse JSON.\""
      "}";

    struct tstr_string result = {
      .string = error,
      .length = sizeof(error) - 1
    };

    return result;
  }

  jsmnf_loader loader;
  jsmnf_pair *pairs = frequenc_safe_malloc((1024 * 1024 * 1) * sizeof(jsmnf_pair)); /* TODO: Replace by auto load */

  jsmnf_init(&loader);
  r = jsmnf_load(&loader, response.body, tokens, parser.toknext, pairs, 1024 * 1024 * 1);
  if (r < 0) {
    printf("[youtube]: Error while loading tokens.\n");

    char error[] =
      "{"
        "\"loadType\":\"error\","
        "\"error\":\"Failed to load JSON tokens.\""
      "}";

    struct tstr_string result = {
      .string = error,
      .length = sizeof(error) - 1,
      .allocated = false
    };

    return result;
  }

  char *path[20] = { "error", "message" };
  jsmnf_pair *error = jsmnf_find_path(pairs, response.body, path, 1);

  if (error != NULL) {
    printf("[youtube]: Error: %.*s\n", (int)error->v.len, response.body + error->v.pos);

    char *errorStr = frequenc_safe_malloc((error->v.len + 1) * sizeof(char));
    frequenc_fast_copy(response.body + error->v.pos, errorStr, error->v.len);

    char errorJSON[1024];
    int errorJSONLength = snprintf(errorJSON, sizeof(errorJSON),
      "{"
        "\"loadType\":\"error\","
        "\"error\":\"%s\""
      "}", errorStr);

    free(errorStr);

    struct tstr_string result = {
      .string = errorJSON,
      .length = errorJSONLength,
      .allocated = false
    };

    return result;
  }

  path[0] = "contents";
  path[1] = "sectionListRenderer";
  path[2] = "contents";

  jsmnf_pair *contents = jsmnf_find_path(pairs, response.body, path, 3);

  char iStr[2];
  snprintf(iStr, sizeof(iStr), "%d", contents->size - 1); /* TODO: Replace snprintf */

  path[3] = iStr;
  path[4] = "itemSectionRenderer";
  path[5] = "contents";

  jsmnf_pair *videos = jsmnf_find_path(pairs, response.body, path, 6);

  char *responseStr = frequenc_safe_malloc((29 + 1) * sizeof(char));
  size_t responseStrLength = snprintf(responseStr, (29 + 1) * sizeof(char),
    "{"
      "\"loadType\":\"search\","
      "\"data\":[");

  int i = 0;
  while (i < videos->size) {
    char i2Str[3];
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

      char *identifierStr = frequenc_safe_malloc((identifier->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + identifier->v.pos, identifierStr, identifier->v.len);

      char *authorStr = frequenc_safe_malloc((author->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + author->v.pos, authorStr, author->v.len);

      char *lengthStr = frequenc_safe_malloc((length->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + length->v.pos, lengthStr, length->v.len);

      size_t lengthInt = 0;

      if (length) {
        int amount = tstr_find_amount(lengthStr, ":");

        switch (amount) {
          case 0: {
            lengthInt += atoi(lengthStr) * 1000;

            break;
          }
          case 1: {
            struct tstr_string_token lengthStrUtils;
            tstr_find_between(&lengthStrUtils, lengthStr, NULL, 0, ":", 0);

            char *lengthStrMinutes = frequenc_safe_malloc((lengthStrUtils.end + 1) * sizeof(char));
            frequenc_fast_copy(lengthStr + lengthStrUtils.start, lengthStrMinutes, lengthStrUtils.end);

            lengthInt += atoi(lengthStrMinutes) * (60 * 1000);

            free(lengthStrMinutes);

            struct tstr_string_token lengthStrUtils2;
            tstr_find_between(&lengthStrUtils2, lengthStr, NULL, lengthStrUtils.end + 1, ":", 0);

            char *lengthStrSeconds = frequenc_safe_malloc((lengthStrUtils2.end - lengthStrUtils2.start + 1) * sizeof(char));
            frequenc_fast_copy(lengthStr + lengthStrUtils2.start, lengthStrSeconds, lengthStrUtils2.end - lengthStrUtils2.start);

            lengthInt += atoi(lengthStrSeconds) * 1000;

            free(lengthStrSeconds);

            break;
          }
          case 2: {
            struct tstr_string_token lengthStrUtils;
            tstr_find_between(&lengthStrUtils, lengthStr, NULL, 0, ":", 0);

            char *lengthStrHours = frequenc_safe_malloc((lengthStrUtils.end + 1) * sizeof(char));
            frequenc_fast_copy(lengthStr + lengthStrUtils.start, lengthStrHours, lengthStrUtils.end);

            lengthInt += atoi(lengthStrHours) * (60 * 60 * 1000);

            free(lengthStrHours);

            struct tstr_string_token lengthStrUtils2;
            tstr_find_between(&lengthStrUtils2, lengthStr, NULL, lengthStrUtils.end + 1, ":", 0);

            char *lengthStrMinutes = frequenc_safe_malloc((lengthStrUtils2.end - lengthStrUtils2.start + 1) * sizeof(char));
            frequenc_fast_copy(lengthStr + lengthStrUtils2.start, lengthStrMinutes, lengthStrUtils2.end - lengthStrUtils2.start);

            lengthInt += atoi(lengthStrMinutes) * (60 * 1000);

            free(lengthStrMinutes);

            struct tstr_string_token lengthStrUtils3;
            tstr_find_between(&lengthStrUtils3, lengthStr, NULL, lengthStrUtils2.end + 1, ":", 0);

            char *lengthStrSeconds = frequenc_safe_malloc((lengthStrUtils3.end - lengthStrUtils3.start + 1) * sizeof(char));
            frequenc_fast_copy(lengthStr + lengthStrUtils3.start, lengthStrSeconds, lengthStrUtils3.end - lengthStrUtils3.start);

            lengthInt += atoi(lengthStrSeconds) * 1000;

            free(lengthStrSeconds);

            break;
          }
        }
      }

      char *titleStr = frequenc_safe_malloc((title->v.len + 1) * sizeof(char));
      frequenc_fast_copy(response.body + title->v.pos, titleStr, title->v.len);

      char *uriStr = frequenc_safe_malloc((sizeof("https://www.youtube.com/watch?v=") - 1 + identifier->v.len + 1) * sizeof(char));
      snprintf(uriStr, sizeof("https://www.youtube.com/watch?v=") - 1 + identifier->v.len + 1, "https://www.youtube.com/watch?v=%s", identifierStr);

      char *artworkUrlStr = frequenc_safe_malloc((sizeof("https://i.ytimg.com/vi/") - 1 + identifier->v.len + sizeof("/maxresdefault.jpg") - 1 + 1) * sizeof(char));
      snprintf(artworkUrlStr, sizeof("https://i.ytimg.com/vi/") - 1 + identifier->v.len + sizeof("/maxresdefault.jpg") - 1 + 1, "https://i.ytimg.com/vi/%s/maxresdefault.jpg", identifierStr);

      struct frequenc_track_info trackInfo = {
        .title = titleStr,
        .author = authorStr,
        .length = lengthInt,
        .identifier = identifierStr,
        .isStream = lengthInt == 0,
        .uri = uriStr,
        .artworkUrl = artworkUrlStr,
        .isrc = NULL,
        .sourceName = "YouTube",
        .position = 0
      };

      char *encodedTrack = NULL;
      encodeTrack(&trackInfo, &encodedTrack);

      char videoJSON[2048];
      frequenc_track_info_to_json(&trackInfo, encodedTrack, videoJSON, sizeof(videoJSON));

      if (responseStrLength == 29) {
        responseStr = realloc(responseStr, responseStrLength + strlen(videoJSON) + 2);

        tstr_append(responseStr, videoJSON, &responseStrLength, 0);
      } else {
        responseStr = realloc(responseStr, responseStrLength + strlen(videoJSON) + sizeof(",") - 1 + 2);

        tstr_append(responseStr, ",", &responseStrLength, 0);
        tstr_append(responseStr, videoJSON, &responseStrLength, 0);
      }

      free(encodedTrack);
      free(identifierStr);
      free(authorStr);
      free(lengthStr);
      free(titleStr);
      free(uriStr);
      free(artworkUrlStr);
    }

    i++;
  }

  if (responseStrLength == 29) {
    free(responseStr);
    free(response.body);
    free(pairs);
    free(tokens);

    char *responseMsg = frequenc_safe_malloc((20 + 1) * sizeof(char));
    snprintf(responseMsg, 20 + 1,
      "{"
        "\"loadType\":\"empty\""
      "}");

    struct tstr_string result = {
      .string = responseMsg,
      .length = 20,
      .allocated = true
    };

    return result;
  }

  responseStr = realloc(responseStr, responseStrLength + (sizeof("]}") - 1) + 1);
  tstr_append(responseStr, "]}", &responseStrLength, 0);

  free(pairs);
  free(tokens);
  httpclient_free(&response);

  struct tstr_string result = {
    .string = responseStr,
    .length = responseStrLength,
    .allocated = true
  };

  return result;
}
