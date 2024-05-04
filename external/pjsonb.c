/*
  PerformanC's JSON builder, inspired on lcsmüller's json-build (https://github.com/lcsmuller/json-build),
  however allocating memory manually.

  License available on: licenses/performanc.license
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pjsonb.h"

void pjsonb_init(struct pjsonb *builder, enum pjsonb_type type) {
  builder->string = malloc(1);
  builder->string[0] = type == PJSONB_ARRAY ? '[' : '{';
  builder->position = 1;
  builder->key_state = PJSONB_NONE;
  builder->type = type;
}

void pjsonb_end(struct pjsonb *builder) {
  if (builder->key_state == PJSONB_TO_CLOSE) {
    builder->string[builder->position - 1] = builder->type == PJSONB_ARRAY ? ']' : '}';
  } else {
    builder->string = realloc(builder->string, builder->position + 1);
    builder->string[builder->position] = builder->type == PJSONB_ARRAY ? ']' : '}';
    builder->position++;
  }
}

void pjsonb_free(struct pjsonb *builder) {
  free(builder->string);
}

void pjsonb_set_int(struct pjsonb *builder, const char *key, int value) {
  int value_length = snprintf(NULL, 0, "%d", value);

  if (key == NULL) {
    builder->string = realloc(builder->string, builder->position + value_length + 1);

    builder->position += snprintf(builder->string + builder->position, value_length + 1, "%d", value);

    memcpy(builder->string + builder->position, ",", 1);

    builder->key_state = PJSONB_TO_CLOSE;

    return;
  }

  int key_length = strlen(key);

  builder->string = realloc(builder->string, builder->position + value_length + 4 + key_length + 1);

  builder->string[builder->position] = '"';
  builder->position++;

  memcpy(builder->string + builder->position, key, key_length);
  builder->position += key_length;

  memcpy(builder->string + builder->position, "\":", 2);
  builder->position += 2;

  builder->position += snprintf(builder->string + builder->position, value_length + 1, "%d", value);

  memcpy(builder->string + builder->position, ",", 1);
  builder->position++;

  builder->key_state = PJSONB_TO_CLOSE;
}

void pjsonb_set_float(struct pjsonb *builder, const char *key, float value) {
  int value_length = snprintf(NULL, 0, "%f", value);

  if (key == NULL) {
    builder->string = realloc(builder->string, builder->position + value_length + 1);

    builder->position += snprintf(builder->string + builder->position, value_length + 1, "%f", value);

    memcpy(builder->string + builder->position, ",", 1);

    builder->key_state = PJSONB_TO_CLOSE;

    return;
  }

  int key_length = strlen(key);

  builder->string = realloc(builder->string, builder->position + value_length + 4 + key_length + 1);

  builder->string[builder->position] = '"';
  builder->position++;

  memcpy(builder->string + builder->position, key, key_length);
  builder->position += key_length;

  memcpy(builder->string + builder->position, "\":", 2);
  builder->position += 2;

  builder->position += snprintf(builder->string + builder->position, value_length + 1, "%f", value);

  memcpy(builder->string + builder->position, ",", 1);
  builder->position++;

  builder->key_state = PJSONB_TO_CLOSE;
}

void pjsonb_set_bool(struct pjsonb *builder, const char *key, int value) {
  int value_length = value ? sizeof("true") -1 : sizeof("false") - 1;

  if (key == NULL) {
    builder->string = realloc(builder->string, builder->position + value_length + 1);

    memcpy(builder->string + builder->position, value ? "true" : "false", value_length);
    builder->position += value_length;

    memcpy(builder->string + builder->position, ",", 1);
    builder->position++;

    builder->key_state = PJSONB_TO_CLOSE;

    return;
  }

  int key_length = strlen(key);

  builder->string = realloc(builder->string, builder->position + value_length + 4 + key_length + 1);

  memcpy(builder->string + builder->position, "\"", 1);
  builder->position++;

  memcpy(builder->string + builder->position, key, key_length);
  builder->position += key_length;

  memcpy(builder->string + builder->position, "\":", 2);
  builder->position += 2;

  memcpy(builder->string + builder->position, value ? "true" : "false", value_length);
  builder->position += value_length;

  memcpy(builder->string + builder->position, ",", 1);
  builder->position++;

  builder->key_state = PJSONB_TO_CLOSE;
}

void pjsonb_set_string(struct pjsonb *builder, const char *key, const char *value, size_t value_length) {
  if (key == NULL) {
    builder->string = realloc(builder->string, builder->position + value_length + 3);

    memcpy(builder->string + builder->position, "\"", 1);
    builder->position++;

    memcpy(builder->string + builder->position, value, value_length);
    builder->position += value_length;

    memcpy(builder->string + builder->position, "\",", 2);
    builder->position += 2;

    builder->key_state = PJSONB_TO_CLOSE;

    return;
  }

  int key_length = strlen(key);

  builder->string = realloc(builder->string, builder->position + value_length + 6 + key_length + 1);

  memcpy(builder->string + builder->position, "\"", 1);
  builder->position++;

  memcpy(builder->string + builder->position, key, key_length);
  builder->position += key_length;

  memcpy(builder->string + builder->position, "\":\"", 3);
  builder->position += 3;

  memcpy(builder->string + builder->position, value, value_length);
  builder->position += value_length;

  memcpy(builder->string + builder->position, "\",", 2);
  builder->position += 2;

  builder->key_state = PJSONB_TO_CLOSE;
}

void pjsonb_enter_object(struct pjsonb *builder, const char *key) {
  if (key == NULL) {
    builder->string = realloc(builder->string, builder->position + 1);
    builder->string[builder->position] = '{';
    builder->position++;
    builder->key_state = PJSONB_NONE;

    return;
  }

  int key_length = strlen(key);

  builder->string = realloc(builder->string, builder->position + 5 + key_length);

  memcpy(builder->string + builder->position, "\"", 1);
  builder->position++;

  memcpy(builder->string + builder->position, key, key_length);
  builder->position += key_length;

  memcpy(builder->string + builder->position, "\":{", 3);
  builder->position += 3;

  builder->key_state = PJSONB_NONE;
}

void pjsonb_leave_object(struct pjsonb *builder) {
  if (builder->key_state == PJSONB_TO_CLOSE) {
    builder->string[builder->position - 1] = '}';
    builder->string = realloc(builder->string, builder->position + 1);
    builder->string[builder->position] = ',';
    builder->position++;
  } else {
    builder->string = realloc(builder->string, builder->position + 2 + 1);

    memcpy(builder->string + builder->position, "},", 2);
    builder->position += 2;
  }

  builder->key_state = PJSONB_TO_CLOSE;
}

void pjsonb_enter_array(struct pjsonb *builder, const char *key) {
  if (key == NULL) {
    builder->string = realloc(builder->string, builder->position + 1);
    builder->string[builder->position] = '[';
    builder->position++;
    builder->key_state = PJSONB_NONE;

    return;
  }

  int key_length = strlen(key);

  builder->string = realloc(builder->string, builder->position + 4 + key_length);

  memcpy(builder->string + builder->position, "\"", 1);
  builder->position++;

  memcpy(builder->string + builder->position, key, key_length);
  builder->position += key_length;

  memcpy(builder->string + builder->position, "\":[", 3);
  builder->position += 3;

  builder->key_state = PJSONB_NONE;
}

void pjsonb_leave_array(struct pjsonb *builder) {
  if (builder->key_state == PJSONB_TO_CLOSE) {
    builder->string[builder->position - 1] = ']';

    builder->key_state = PJSONB_NONE;
  } else {
    builder->string = realloc(builder->string, builder->position + 1);

    builder->string[builder->position] = ']';
    builder->position++;
  }
}
