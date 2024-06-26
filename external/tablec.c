/*
  (PerformanC's) TableC: Simple hashtable library for C.

  License available on: licenses/performanc.license
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tablec.h"

void tablec_init(struct tablec_ht *tablec, size_t max_capacity) {
  tablec->length = 0;
  tablec->capacity = max_capacity;
  tablec->buckets = calloc(sizeof(struct tablec_bucket) * max_capacity, 1);
}

static size_t __tablec_hash(struct tablec_ht *tablec, char *key) {
  size_t hash = 0, i = 0;

  while (key[i] != '\0') hash = hash * 37 + (key[i++] & 255);

  return hash % tablec->capacity;
}

void tablec_resize(struct tablec_ht *tablec, size_t new_max_capacity) {
  struct tablec_ht newHashtable;
  tablec_init(&newHashtable, new_max_capacity);

  while (tablec->capacity--) {
    if (!tablec->buckets[tablec->capacity].key) continue;
      
    tablec_set(&newHashtable, tablec->buckets[tablec->capacity].key, tablec->buckets[tablec->capacity].value);
  }

  tablec_cleanup(tablec);
  tablec->capacity = newHashtable.capacity;
  tablec->length = newHashtable.length;
  tablec->buckets = newHashtable.buckets;
}

void tablec_set(struct tablec_ht *tablec, char *key, void *value) {
  size_t hash;

  if (tablec->length - 1 == tablec->capacity) tablec_resize(tablec, tablec->capacity * 2);

  hash = __tablec_hash(tablec, key);

  while (hash != tablec->capacity) {
    if (!tablec->buckets[hash].key) {
      tablec->buckets[hash].key = key;
      tablec->buckets[hash].value = value;

      tablec->length++;

      return;
    }

    hash++;
  }

  tablec_resize(tablec, tablec->capacity * 2);
  tablec_set(tablec, key, value);

  return;
}

void tablec_del(struct tablec_ht *tablec, char *key) {
  size_t hash = __tablec_hash(tablec, key);

  while (hash != tablec->capacity) {
    if (tablec->buckets[hash].key && strcmp(tablec->buckets[hash].key, key) == 0) {
      tablec->buckets[hash].key = NULL;
      tablec->buckets[hash].value = NULL;

      tablec->length--;

      return;
    }

    hash++;
  }
}

struct tablec_bucket *tablec_get(struct tablec_ht *tablec, char *key) {
  size_t hash = __tablec_hash(tablec, key);

  while (hash != tablec->capacity) {
    if (tablec->buckets[hash].key && strcmp(tablec->buckets[hash].key, key) == 0)
      return &tablec->buckets[hash];

    hash++;
  }

  return NULL;
}

int tablec_full(struct tablec_ht *tablec) {
  return (int)tablec->capacity == (int)tablec->length ? -1 : (int)tablec->capacity - (int)tablec->length;
}

void tablec_cleanup(struct tablec_ht *tablec) {
  free(tablec->buckets);
}
