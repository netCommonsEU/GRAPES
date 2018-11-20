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
#include <stdio.h>
#include <assert.h>

#include "chunk.h"
#include "chunkidms_private.h"
#include "chunkidss_ops.h"
#include "chunkidms.h"
#include "chunkbuffer.h"
#include "grapes_config.h"
#include "int_coding.h"


#define DEFAULT_SIZE_INCREMENT 4

struct chunkID_multiSet *chunkID_multiSet_init(int size, int single_size)
{
  struct chunkID_multiSet *p;
  p = malloc(sizeof(struct chunkID_multiSet));
  if (p == NULL) {
    return NULL;
  }
  p->max_size = 0;
  p->single_size = 0;
  p->size = 0;
  p->cache = NULL;

  p->max_size = size>0 ? size : 0;
  p->single_size = single_size>0 ? single_size : 0;

  if (p->max_size) {
    p->sets = malloc(p->max_size * sizeof( struct chunkID_singleSet *));
    if (p->sets == NULL) {
      p->max_size = 0;
    }
  } else {
    p->sets = NULL;
  }

  return p;
}


void chunkID_multiSet_free(struct chunkID_multiSet *h)
{
  int i;

  if(!h)
    return;

  for(i=0; i < h->size; i++)
  {
    chunkID_singleSet_free(h->sets[i]);
  }
  free(h->sets);
  free(h);
}


static struct chunkID_singleSet * getSet(struct chunkID_multiSet *h, flowid_t flow_id)
{

  int i;
  struct chunkID_singleSet *res=NULL;

  if(!h)
  return NULL;

  if(h->cache && h->cache->flow_id == flow_id)
    return h->cache;

  for( i=0; i<h->size; i++)
    if(h->sets[i]->flow_id == flow_id)
      res = h->sets[i];

  if(!res && h->size == h->max_size)
  {
      h->max_size += DEFAULT_SIZE_INCREMENT;
      h->sets = realloc(h->sets, h->max_size * sizeof( struct chunkID_singleSet * ));
      if (h->sets == NULL) {
        h->size = 0;
        return NULL;
      }
  }
  if( !res && h->sets && h->size < h->max_size)
  {
    res = h->sets[h->size] = chunkID_singleSet_init(h->single_size,flow_id);
    h->size+=1;
  }

  h->cache = res;

  return res;

}

int chunkID_multiSet_add_chunk(struct chunkID_multiSet *h, int chunk_id, flowid_t flow_id)
{
  struct chunkID_singleSet *res=getSet(h, flow_id);
  if(res)
    return chunkID_singleSet_add_chunk(res, chunk_id);
  else
    return -1;
}


int chunkID_multiSet_single_size(struct chunkID_multiSet *h, flowid_t flow_id)
{
  struct chunkID_singleSet *res=getSet(h, flow_id);
  if(res)
    return res->n_elements;
  else
    return -1;
}


int chunkID_multiSet_size(const struct chunkID_multiSet *h)
{

  if(!h)
    return 0;
  return h->size;
}

int chunkID_multiSet_total_size(const struct chunkID_multiSet *h)
{
  int i, res=0;

  if(!h)
    return 0;

  for(i=0; i<h->size; i++)
    res += h->sets[i]->n_elements;

  return res;
}



int chunkID_multiSet_get_chunk(struct chunkID_multiSet *h, int i, flowid_t flow_id)
{
  struct chunkID_singleSet *res=getSet(h, flow_id);
  if (res && i < res->n_elements) {
    return res->elements[i];
  }

  return CHUNKID_INVALID;
}


int chunkID_multiSet_check(struct chunkID_multiSet *h, int chunk_id, flowid_t flow_id)
{
  struct chunkID_singleSet *res=getSet(h, flow_id);
  if (res) {
    return chunkID_singleSet_check(res, chunk_id);
  }

  return -1;


}
void chunkID_multiSet_clear(struct chunkID_multiSet *h, int size, flowid_t flow_id)
{

  struct chunkID_singleSet *res=getSet(h, flow_id);
  if (res) {
    chunkID_singleSet_clear(res,size);
  }

}


void chunkID_multiSet_clear_all(struct chunkID_multiSet *h, int size)
{
  int i=0;

  if(!h)
    return;
  for(; i< h->size; i++)
  {
    if (h->sets[i]) {
      chunkID_singleSet_clear(h->sets[i], size);
    }
  }

}


void chunkID_multiSet_trim(struct chunkID_multiSet *h, int size, flowid_t flow_id)
{

  struct chunkID_singleSet *res=getSet(h, flow_id);

  if(res)
    chunkID_singleSet_trim(res,size);
}

void chunkID_multiSet_trimAll(struct chunkID_multiSet *h, int size)
{
        int i;
        for(i=0; i< h->size; i++)
                chunkID_singleSet_trim(h->sets[i], size);
}


uint32_t chunkID_multiSet_get_earliest(struct chunkID_multiSet *h, flowid_t flow_id)
{
  struct chunkID_singleSet *res=getSet(h, flow_id);

  if(res && res->size >0)
  {
    return chunkID_singleSet_get_earliest(res);
  }
  else
  {
    return CHUNKID_INVALID;
  }

}

uint32_t chunkID_multiSet_get_latest(struct chunkID_multiSet *h, flowid_t flow_id)
{

    struct chunkID_singleSet *res=getSet(h, flow_id);

    if(res && res->size >0)
    {
      return chunkID_singleSet_get_latest(res);
    }
    else
    {
      return CHUNKID_INVALID;
    }
}

void chunkID_multiSet_union(struct chunkID_multiSet *h, struct chunkID_multiSet *a)
{

  struct chunkID_singleSet *s;
  int i;

  if(!h || !a)
    return;

  if(chunkID_multiSet_total_size(a)>0)
  {
    for(i=0; i<a->size; i++)
    {
      s = getSet(h, a->sets[i]->flow_id);
      if(s)
        chunkID_singleSet_union(s, a->sets[i]);
    }
  }
}


/* Converts a chunk_buffer into a ChunkIDSingleSet
*
*/
static struct chunkID_singleSet *cb_to_bmap_single(const struct chunk_buffer *cb)
{
  struct chunk *chunks;
  int num_chunks, i;
  struct chunkID_singleSet *my_bmap;

  if(!cb)
    return NULL;

  chunks = cb_get_chunks(cb, &num_chunks);
  my_bmap = chunkID_singleSet_init(num_chunks, cb_get_flowid(cb));

  for(i=num_chunks-1; i>=0; i--) {
    chunkID_singleSet_add_chunk(my_bmap, chunks[i].id);
  }
  return my_bmap;
}

struct chunkID_multiSet *generateChunkIDMultiSetFromChunkBuffers(struct chunk_buffer ** cbs, int len)
{
  int i;
  struct chunkID_multiSet *ms;


  if(!cbs)
  return NULL;

  ms = chunkID_multiSet_init(len,0);
  if(!ms || !ms->sets || ms->max_size!=len)
    return NULL;

  for(i=0; i<len; i++)
    ms->sets[i]=cb_to_bmap_single(cbs[i]);

  ms->size=len;
  return ms;
}


void chunkID_multiSet_print(FILE * stream, const struct chunkID_multiSet * ms)
{

  int i,j;
  if(!ms)
  {
    fprintf(stream, "The multiset is empty!!\n");
    return;
  }

  fprintf(stream, "ChunkID_multiSet | size : %d | max_size: %d | single_size: %d\n", ms->size, ms->max_size, ms->single_size );
  for(i=0; i<ms->size; i++)
  {
    fprintf(stream, "Set %d  |  flow_id: %d  |  %d elements\n", i, ms->sets[i]->flow_id, ms->sets[i]->n_elements);
    for(j=0; j<ms->sets[i]->n_elements; j++)
    {
      fprintf(stream, "%d\t", ms->sets[i]->elements[j]);
      }
    fprintf(stream, "\n");

  }
  fprintf(stream, "\n-----------------------------------------------------------\n");
}


void chunkID_multiSet_set_cached(struct chunkID_multiSet * ms, flowid_t flow_id)
{
  getSet(ms, flow_id);
}

int chunkID_multiSet_get_cached(struct chunkID_multiSet * ms)
{
  if(ms->cache)
    return ms->cache->flow_id;
  else
    return -1;
}



int * chunkID_multiSet_get_flows(struct chunkID_multiSet *ms, int * size)
{
  if(ms)
  {

    int i;
    int *res= malloc(sizeof(int) * ms->size);

    for(i=0; i<ms->size; i++)
      res[i]=ms->sets[i]->flow_id;

    *size=ms->size;
    return res;
  }
  else
  {
    return NULL;
  }
}




//
// void singleset_encoding_test()
// {
//   struct chunkID_singleSet *s= chunkID_singleSet_init(10,17);
//   struct chunkID_singleSet *s1=NULL;
//   static uint8_t buff[2048];
//
//   uint8_t *meta;
//   uint8_t *meta2;
//   int i,a=0,size;
//   if(!s)
//   {
//     printf("Error creating set!!!\n");
//     return;
//   }
//   for(i=0; i<10; i++)
//     chunkID_singleSet_add_chunk(s,50+i);
//
//   for(i=0; i<s->n_elements; i++)
//     printf("%d ", s->elements[i]);
//
//   printf("\n");
//
//   meta=chunkID_singleSet_encode(s, buff, sizeof(buff),0);
//   printf("Encoding: %ld bytes\n", 1+meta-buff);
//   for(i=0; i<meta-buff; i++)
//     printf("%hhx ",buff[i]);
//   printf("\n");
//   size = int_rcpy(buff);
//   s1 = chunkID_singleSet_init(size, 17);
//
//   meta2=chunkID_singleSet_decode(s1,buff,meta-buff, &a);
//   printf("Decoding: %ld bytes\n", 1+meta-buff);
//   printf("Size: %d\n", s1->n_elements);
//   printf("Flow_id: %d\n", s1->flow_id);
//   printf("Meta is after %ld bytes from start\n", meta2-buff);
//
//   for(i=0; i<s1->n_elements; i++)
//     printf("%d ", s1->elements[i]);
//
//   printf("\n");
//
//
// }
