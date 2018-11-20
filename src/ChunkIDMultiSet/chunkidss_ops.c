/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include "chunkidms_private.h"
#include "chunkidms.h"

#include "grapes_config.h"

#define DEFAULT_SIZE_INCREMENT 32

struct chunkID_singleSet *chunkID_singleSet_init(const int size, const flowid_t flow_id)
{
  struct chunkID_singleSet *p;

  p = malloc(sizeof(struct chunkID_singleSet));
  if (p == NULL) {
    return NULL;
  }
  p->size= size>0 ? size : 0;
  p->n_elements=0;
  p->flow_id=flow_id;

  if (p->size) {
    p->elements = malloc(p->size * sizeof( int *));
    if (p->elements== NULL) {
      p->size = 0;
    }
  } else {
    p->elements = NULL;
  }

  return p;
}


static int int_cmp(const void *pa, const void *pb)
{
  return (*(const int *)pa - *(const int *)pb);
}

static int check_insert_pos(const struct chunkID_singleSet *h, chunkid_t id)
{
  int a, b, c, r;

  if (! h->n_elements) {
    return 0;
  }

  a = 0;
  b = c = h->n_elements - 1;

  while ((r = int_cmp(&id, &h->elements[b])) != 0) {
    if (r > 0) {
      if (b == c) {
        return b + 1;
      } else {
        a = b + 1;
      }
    } else {
      if (b == a) {
        return b;
      } else {
        c = b;
      }
    }
    b = (a + c) / 2;
  }

  return -1;
}

static int chunkID_singleSet_get_chunk(const struct chunkID_singleSet *c, int i)
{
  if(c)
    return c->elements[i];
  else
    return -1;
}


int chunkID_singleSet_add_chunk(struct chunkID_singleSet *h, int chunk_id)
{
  int pos;

  pos = check_insert_pos(h, chunk_id);
  if (pos < 0){
    return 0;
  }

  if (h->n_elements == h->size) {
    int *res;

    res = realloc(h->elements, (h->size + DEFAULT_SIZE_INCREMENT) * sizeof(int));
    if (res == NULL) {
      return -1;
    }
    h->size += DEFAULT_SIZE_INCREMENT;
    h->elements = res;
  }

  memmove(&h->elements[pos + 1], &h->elements[pos] , ((h->n_elements++) - pos) * sizeof(int));

  h->elements[pos] = chunk_id;

  return h->n_elements;
}

int chunkID_singleSet_check(const struct chunkID_singleSet *h, int chunk_id)
{
  int *p;

  p = bsearch(&chunk_id, h->elements, (size_t) h->n_elements, sizeof(h->elements[0]), int_cmp);
  return p ? p - h->elements : -1;
}


void chunkID_singleSet_clear(struct chunkID_singleSet *h, int size)
{
  h->n_elements = 0;
  h->size = size;
  h->elements = realloc(h->elements, size * sizeof(int));
  if (h->elements == NULL) {
    h->size = 0;
  }
}

void chunkID_singleSet_free(struct chunkID_singleSet *h)
{
  chunkID_singleSet_clear(h,0);
  free(h->elements);
  free(h);
}

void chunkID_singleSet_trim(struct chunkID_singleSet *h, int size)
{
  if (h->n_elements > size) {
    memmove(h->elements, h->elements + h->n_elements - size, sizeof(h->elements[0]) * size);
    h->n_elements = size;
  }
}


uint32_t chunkID_singleSet_get_earliest(const struct chunkID_singleSet *h)
{
  int i;
  uint32_t min;

  if (h->n_elements == 0) {
    return CHUNKID_INVALID;
  }
  min = chunkID_singleSet_get_chunk(h, 0);
  for (i = 1; i < h->n_elements; i++) {
    int c = chunkID_singleSet_get_chunk(h, i);

    min = (c < min) ? c : min;
  }

  return min;
}

uint32_t chunkID_singleSet_get_latest(const struct chunkID_singleSet *h)
{
  int i;
  uint32_t  max;

  if (h->n_elements == 0) {
    return CHUNKID_INVALID;
  }
  max = chunkID_singleSet_get_chunk(h, 0);
  for (i = 1; i < h->n_elements; i++) {
    int c = chunkID_singleSet_get_chunk(h, i);

    max = (c > max) ? c : max;
  }

  return max;
}

int chunkID_singleSet_union(struct chunkID_singleSet *h, struct chunkID_singleSet *a)
{
  int i;

  for (i = 0; i < a->n_elements; i++) {
    int ret = chunkID_singleSet_add_chunk(h, a->elements[i]);
    if (ret < 0) return ret;
  }

  return h->size;
}
