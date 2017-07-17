/*
 * peer.c
 *
 *  Created on: Mar 29, 2010
 *      Author: Marco Biazzini
 */

#include <stdio.h>

#include <sys/time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "net_helper.h"
#include "chunkidset.h"
#include "peer.h"

struct peer *peerCreate (struct nodeID *id, int *size)
{
	struct peer *res;

	*size = sizeof(struct peer);
	res = (struct peer *)malloc(*size);
	res->id = nodeid_dup(id);
	gettimeofday(&(res->creation_timestamp),NULL);
	return res;
}

void peerDelete (struct peer *p) {
	nodeid_free(p->id);
	free(p);
}

// serialize nodeID and cb_size only
int peerDump(const struct peer *p, uint8_t **buf) {

	uint8_t id[256];
	int id_size = nodeid_dump(id, p->id, 256);
	int res = id_size+sizeof(int);
	*buf = realloc(*buf, res);
	memcpy(*buf,id,id_size);

	return res;

}


struct peer* peerUndump(const uint8_t *buf, int *size) {

	struct peer *p;
	p = malloc(sizeof(struct peer));
	p->id = nodeid_undump(buf,size);
	*size = *size + sizeof(int);

	gettimeofday(&(p->creation_timestamp),NULL);

	return p;

}

