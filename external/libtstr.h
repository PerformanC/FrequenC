/*
  libtstr: PerformanC's string utilities (library token string)

  License available on: licenses/performanc.license
*/

#ifndef LIBTSTR_H
#define LIBTSTR_H

#include <stdlib.h> // size_t
#include "types.h"

struct tstr_string_token {
  int start;
  int end;
};

struct tstr_string {
  char *string;
  size_t length;
  bool allocated;
};

void tstr_find_between(struct tstr_string_token *result, const char *str, const char *start, int start_index, const char *end, int len);

void tstr_append(char *str, const char *append, size_t *length, int limiter);

int tstr_find_amount(const char *str, const char *find);

void tstr_free(struct tstr_string *str);

#endif
