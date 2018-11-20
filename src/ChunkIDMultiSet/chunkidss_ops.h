/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include "chunkidms_private.h"

struct chunkID_singleSet *chunkID_singleSet_init(const int size, const flowid_t flow_id);

int chunkID_singleSet_add_chunk(struct chunkID_singleSet *h, int chunk_id);


int chunkID_singleSet_check(const struct chunkID_singleSet *h, int chunk_id);


void chunkID_singleSet_clear(struct chunkID_singleSet *h, int size);


int chunkID_singleSet_union(struct chunkID_singleSet *h, struct chunkID_singleSet *a);

void chunkID_singleSet_free(struct chunkID_singleSet *h);


void chunkID_singleSet_trim(struct chunkID_singleSet *h, int size);


uint32_t chunkID_singleSet_get_latest(const struct chunkID_singleSet *h);


uint32_t chunkID_singleSet_get_earliest(const struct chunkID_singleSet *h);




const uint8_t *chunkID_singleSet_decode(struct chunkID_singleSet *h, const uint8_t *buff, int buff_len, int *meta_len);


uint8_t *chunkID_singleSet_encode(const struct chunkID_singleSet *h, uint8_t *buff, int buff_len, int meta_len);
