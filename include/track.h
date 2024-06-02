#ifndef TRACK_H
#define TRACK_H

#include <stdint.h>

#include "libtstr.h"
#include "pjsonb.h"
#define JSMN_HEADER
#define JSMN_STRICT /* Set strict mode for jsmn (JSON tokenizer) */
#include "jsmn.h"
#include "jsmn-find.h"

#include "types.h"

struct frequenc_track_info {
  struct tstr_string title;
  struct tstr_string author;
  size_t length;
  struct tstr_string identifier;
  bool is_stream;
  struct tstr_string uri;
  struct tstr_string artwork_url;
  struct tstr_string isrc;
  struct tstr_string source_name;
};

int frequenc_decode_track(struct frequenc_track_info *result, const struct tstr_string *track);

void frequenc_free_track(struct frequenc_track_info *track);

void frequenc_encode_track(struct frequenc_track_info *track, struct tstr_string *result);

void frequenc_track_info_to_json(struct frequenc_track_info *track_info, struct tstr_string *encoded, struct pjsonb *track_json, bool unique);

void frequenc_partial_track_info_to_json(struct frequenc_track_info *track_info, struct pjsonb *track_json);

int frequenc_json_to_track_info(struct frequenc_track_info *track_info, jsmnf_pair *pairs, char *json, char *path[], unsigned int path_len, unsigned int path_size);

void frequenc_free_track_info(struct frequenc_track_info *track_info);

#endif
