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
#include "chunkidms.h"
#include "chunkidss_ops.h"
#include "int_coding.h"

int encodeChunkMSSignaling(const struct chunkID_multiSet *f, const void *meta, int meta_len, uint8_t *buff, int buff_len)
{
  uint8_t *meta_p;
  int i;
  int s;
  if(f && f->size>0)
  {
    s=f->size;
    int_cpy(buff+4, meta_len);
    //Use meta_p as iterator through the buffer
    meta_p=buff+8;

    for(i = 0; i<f->size; i++)
    {
      if(f->sets[i]->n_elements >0)
      {
        int_cpy(meta_p, f->sets[i]->size);
        int_cpy(meta_p + 4, f->sets[i]->flow_id);
        meta_p = chunkID_singleSet_encode(f->sets[i], meta_p, buff_len - (meta_p-buff), meta_len);
      }
      else
      {
        s-=1;  //Empty buffer -> don't send it
      }
    }

    int_cpy(buff, s); //How much elements this message carries

  }
  else
  {
    int_cpy(buff, 0);
    int_cpy(buff+4, meta_len);
    meta_p = buff + 8;
  }

  if (meta_p == NULL) {
    return -1;
  }

  if (meta_len) {
    memcpy(meta_p, meta, meta_len);
  }

  return meta_p + meta_len - buff;
}

struct chunkID_multiSet *decodeChunkMSSignaling(void **meta, int *meta_len, const uint8_t *buff, int buff_len)
{
  uint32_t n_sets;
  uint32_t flow_id;
  uint32_t elements;
  struct chunkID_multiSet *ms;
  const uint8_t *meta_p;
  int i;

  n_sets = int_rcpy(buff);
  *meta_len = int_rcpy(buff + 4);
  meta_p=buff+8;

  if(n_sets>0)
  {
    ms=chunkID_multiSet_init(n_sets,0);
    ms->size=n_sets;
    for(i=0; i<n_sets; i++)
    {

      elements = int_rcpy(meta_p);
      flow_id = int_rcpy(meta_p + 4);

      ms->sets[i] = chunkID_singleSet_init(elements, flow_id);

      if (ms->sets[i] == NULL) {
        fprintf(stderr, "Error in decoding chunkid set - not enough memory to create a chunkID set.\n");
        return NULL;
      }

      meta_p = chunkID_singleSet_decode(ms->sets[i], meta_p, buff_len - (buff-meta_p), meta_len);

    }
  }
  else
  {
    ms = NULL;
    meta_p = buff + 8;
  }

  if (*meta_len) {
    *meta = malloc(*meta_len);
    if (*meta != NULL) {
      memcpy(*meta, meta_p, *meta_len);
    } else {
      *meta_len = 0;
    }
  } else {
    *meta = NULL;
  }

  return ms;
}
