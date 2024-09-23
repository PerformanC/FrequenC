/*
  (PerformanC's) Query Parser: Minimalistic query parser for C.

  License available on: licenses/performanc.license
*/

#ifndef URLPARSER_H
#define URLPARSER_H

struct qparser_query {
  char key[64];
  char value[1024];
};

struct qparser_info {
  int length;
  int queries_length;
  struct qparser_query *queries;
};

void qparser_init(struct qparser_info *parse_info, struct qparser_query *buffer, int length);

void qparser_parse(struct qparser_info *parse_info, char *url);

struct qparser_query *qparser_get_query(struct qparser_info *parse_info, char *key);

#endif
