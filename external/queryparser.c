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

void qparser_init(struct qparser_info *parseInfo, struct qparser_query *buffer, int length) {
  parseInfo->length = length;
  parseInfo->queriesLength = 0;
  parseInfo->queries = buffer;
  memset(parseInfo->queries, 0, length * sizeof(struct qparser_query));
}

void qparser_parse(struct qparser_info *parseInfo, char *url) {
  int keyIndex = 0;
  int valueIndex = 0;

  int state = -1;

  for (int i = 0; url[i] != '\0'; i++) {
    if (parseInfo->queriesLength >= parseInfo->length) {
      printf("[queryparser]: qparser_query length exceeded.\n - Limit: %d\n", parseInfo->length);

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

              parseInfo->queries[parseInfo->queriesLength].value[valueIndex] = '\0';

              state = 0;
              valueIndex = 0;
              parseInfo->queriesLength++;

              if (parseInfo->queriesLength >= parseInfo->length) goto queriesLimitReached;
            }

            if (keyIndex >= 64) goto keyLimitReached;
      
            parseInfo->queries[parseInfo->queriesLength].key[keyIndex] = url[i];

            keyIndex++;

            break;
          }
          case QUERY_ON_VALUE:
          case QUERY_ON_VALUE_KEY_END: {
            if (valueIndex >= 1024) goto valueLimitReached;

            if (state == 3) {
              parseInfo->queries[parseInfo->queriesLength].key[keyIndex] = '\0';

              state = 2;
              keyIndex = 0;
            }

            parseInfo->queries[parseInfo->queriesLength].value[valueIndex] = url[i];

            valueIndex++;
          }
        }
      }
    }
  }

  if (parseInfo->queries[parseInfo->queriesLength].key[0] != '\0') {
    if (keyIndex >= 64) goto keyLimitReached;

    parseInfo->queries[parseInfo->queriesLength].value[valueIndex] = '\0';

    parseInfo->queriesLength++;
  }

  return;

  keyLimitReached: {
    printf("[queryparser]: Key length exceeded.\n - Limit: %d\n", parseInfo->length);

    return;
  }

  valueLimitReached: {
    printf("[queryparser]: Value length exceeded.\n - Limit: %d\n", parseInfo->length);

    return;
  }

  queriesLimitReached: {
    printf("[queryparser]: Queries length exceeded.\n - Limit: %d\n", parseInfo->length);

    return;
  }
}

struct qparser_query *qparser_get_query(struct qparser_info *parseInfo, char *key) {
  int i = 0;
  while (i < parseInfo->queriesLength) {
    if (parseInfo->queries[i].key[0] != '\0' && strcmp(parseInfo->queries[i].key, key) == 0) return &parseInfo->queries[i];

    i++;
  }

  return NULL;
}
