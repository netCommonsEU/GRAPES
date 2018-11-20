/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *  This is free software; see gpl-3.0.txt
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "chunkiser_iface.h"
#include "grapes_config.h"

struct chunkiser_ctx {
  char buff[80];
};

static struct chunkiser_ctx *open(const char *fname, int *period, const char *config)
{
  struct chunkiser_ctx *ctx = malloc(sizeof(struct chunkiser_ctx));

  fprintf(stderr, "WARNING: This is a dummy chunkiser, only good for debugging! Do not expect anything good from it!\n");

  *period = 40000;
  return ctx;
}

static void close(struct chunkiser_ctx *s)
{
  free(s);
}

static uint8_t *chunkise(struct chunkiser_ctx *s, chunkid_t id, chunksize_t *size, uint64_t *ts, void **attr, chunksize_t *attr_size)
{
  sprintf(s->buff, "Chunk %d", id);
  *ts = 40 * id * 1000;
  *size = strlen(s->buff);
  return strdup(s->buff);
}

struct chunkiser_iface in_dummy = {
  .open = open,
  .close = close,
  .chunkise = chunkise,
};
