#ifndef CHUNKID_MUlTISET_PRIVATE
#define CHUNKID_MUlTISET_PRIVATE
/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include<chunk.h>

struct chunkID_singleSet {
  uint32_t size;
  uint32_t n_elements;
  int *elements;
  flowid_t flow_id;
};


struct chunkID_multiSet {
  uint32_t size;
  uint32_t max_size;
  uint32_t single_size;
  struct chunkID_singleSet **sets;
  struct chunkID_singleSet *cache;

};




#endif /* CHUNKID_MUlTISET_PRIVATE */
