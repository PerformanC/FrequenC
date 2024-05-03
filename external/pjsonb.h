/*
  PerformanC's JSON builder, inspired on lcsmüller's json-build (https://github.com/lcsmuller/json-build),
  however allocating memory manually.

  License available on: licenses/performanc.license
*/

#ifndef PJSONB_H
#define PJSONB_H

enum pjsonb_key_state {
  PJSONB_NONE,
  PJSONB_TO_CLOSE
};

enum pjsonb_type {
  PJSONB_ARRAY,
  PJSONB_OBJECT
};

struct pjsonb {
  char *string;
  int position;
  enum pjsonb_key_state key_state;
  enum pjsonb_type type;
};

void pjsonb_init(struct pjsonb *builder, enum pjsonb_type type);

void pjsonb_end(struct pjsonb *builder);

void pjsonb_free(struct pjsonb *builder);

void pjsonb_set_int(struct pjsonb *builder, const char *key, int value);

void pjsonb_set_float(struct pjsonb *builder, const char *key, float value);

void pjsonb_set_bool(struct pjsonb *builder, const char *key, int value);

void pjsonb_set_string(struct pjsonb *builder, const char *key, const char *value);

#define pjsonb_set_if(builder, type, condition, key, value) \
  if (condition) pjsonb_set_ ## type(builder, key, value)

void pjsonb_enter_object(struct pjsonb *builder, const char *key);

void pjsonb_leave_object(struct pjsonb *builder);

void pjsonb_enter_array(struct pjsonb *builder, const char *key);

void pjsonb_leave_array(struct pjsonb *builder);

#endif
