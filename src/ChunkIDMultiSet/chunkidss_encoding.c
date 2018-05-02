/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *  This is free software; see lgpl-2.1.txt
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "chunkidms_private.h"
#include "chunkidss_ops.h"
#include "int_coding.h"

uint8_t *chunkID_singleSet_encode(const struct chunkID_singleSet *h, uint8_t *buff, int buff_len, int meta_len)
{
  int i, elements;
  uint32_t c_min, c_max;

  c_min = c_max = h->n_elements ? h->elements[0] : 0;
  for (i = 1; i < h->n_elements; i++) {
    if (h->elements[i] < c_min)
      c_min = h->elements[i];
    else if (h->elements[i] > c_max)
      c_max = h->elements[i];
  }
  elements = h->n_elements ? c_max - c_min + 1 : 0;
  int_cpy(buff, elements);
  elements = elements / 8 + (elements % 8 ? 1 : 0);
  if (buff_len < elements + 12 + meta_len) {
    return NULL;
  }
  int_cpy(buff+8, c_min); //first value in the bitmap, i.e., base value
  memset(buff + 12, 0, elements);
  for (i = 0; i < h->n_elements; i++) {
    buff[12 + (h->elements[i] - c_min) / 8] |= 1 << ((h->elements[i] - c_min) % 8);
  }

  return buff + 12 + elements;
}

const uint8_t *chunkID_singleSet_decode(struct chunkID_singleSet *h, const uint8_t *buff, int buff_len, int *meta_len)
{
  int i;
  int base;
  int byte_cnt;

  byte_cnt = h->size / 8 + (h->size % 8 ? 1 : 0);
  if (buff_len < 8 + byte_cnt + *meta_len) {
    fprintf(stderr, "Error in decoding chunkid set - wrong length\n");
    chunkID_singleSet_free(h);

    return NULL;
  }
  base = int_rcpy(buff+8);
  for (i = 0; i < h->size; i++) {
  if (buff[12 + (i / 8)] & 1 << (i % 8))
    h->elements[h->n_elements++] = base + i;
  }

  return buff + 12 + byte_cnt;
}
