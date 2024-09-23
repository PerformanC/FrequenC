/*
  (PerformanC's) Query Parser: Minimalistic query parser for C.

  License available on: licenses/performanc.license
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "queryparser.h"

#define QUERY_ON_KEY 0
#define QUERY_ON_KEY_VALUE_END 1
#define QUERY_ON_VALUE 2
#define QUERY_ON_VALUE_KEY_END 3

void qparser_init(struct qparser_info *parse_info, struct qparser_query *buffer, int length) {
  parse_info->length = length;
  parse_info->queries_length = 0;
  parse_info->queries = buffer;
  memset(parse_info->queries, 0, (size_t)length * sizeof(struct qparser_query));
}

void qparser_parse(struct qparser_info *parse_info, char *url) {
  int keyIndex = 0;
  int valueIndex = 0;

  int state = -1;

  for (int i = 0; url[i] != '\0'; i++) {
    if (parse_info->queries_length >= parse_info->length) {
      printf("[queryparser]: qparser_query length exceeded.\n - Limit: %d\n", parse_info->length);

      return;
    }

    switch (url[i]) {
      case '?': {
        state = QUERY_ON_KEY;

        break;
      }
      case '=': {
        state = QUERY_ON_VALUE_KEY_END;

        break;
      }
      case '&': {
        state = QUERY_ON_KEY_VALUE_END;

        break;
      }
      default: {
        switch (state) {
          case QUERY_ON_KEY:
          case QUERY_ON_KEY_VALUE_END: {
            if (state == 1) {
              if (keyIndex >= 64) goto keyLimitReached;

              parse_info->queries[parse_info->queries_length].value[valueIndex] = '\0';

              state = 0;
              valueIndex = 0;
              parse_info->queries_length++;

              if (parse_info->queries_length >= parse_info->length) goto queriesLimitReached;
            }

            if (keyIndex >= 64) goto keyLimitReached;
      
            parse_info->queries[parse_info->queries_length].key[keyIndex] = url[i];

            keyIndex++;

            break;
          }
          case QUERY_ON_VALUE:
          case QUERY_ON_VALUE_KEY_END: {
            if (valueIndex >= 1024) goto valueLimitReached;

            if (state == 3) {
              parse_info->queries[parse_info->queries_length].key[keyIndex] = '\0';

              state = 2;
              keyIndex = 0;
            }

            parse_info->queries[parse_info->queries_length].value[valueIndex] = url[i];

            valueIndex++;
          }
        }
      }
    }
  }

  if (parse_info->queries[parse_info->queries_length].key[0] != '\0') {
    if (keyIndex >= 64) goto keyLimitReached;

    parse_info->queries[parse_info->queries_length].value[valueIndex] = '\0';

    parse_info->queries_length++;
  }

  return;

  keyLimitReached: {
    printf("[queryparser]: Key length exceeded.\n - Limit: %d\n", parse_info->length);

    return;
  }

  valueLimitReached: {
    printf("[queryparser]: Value length exceeded.\n - Limit: %d\n", parse_info->length);

    return;
  }

  queriesLimitReached: {
    printf("[queryparser]: Queries length exceeded.\n - Limit: %d\n", parse_info->length);

    return;
  }
}

struct qparser_query *qparser_get_query(struct qparser_info *parse_info, char *key) {
  int i = 0;
  while (i < parse_info->queries_length) {
    if (parse_info->queries[i].key[0] != '\0' && strcmp(parse_info->queries[i].key, key) == 0) return &parse_info->queries[i];

    i++;
  }

  return NULL;
}
