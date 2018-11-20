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
  flowid_t flow_id;
};

static struct chunkiser_ctx *open(const char *fname, int *period, const char *config)
{
  struct chunkiser_ctx *ctx = malloc(sizeof(struct chunkiser_ctx));
  struct tag *cfg_tags;

  fprintf(stderr, "WARNING: This is a dummy chunkiser, only good for debugging! Do not expect anything good from it!\n");

  cfg_tags = grapes_config_parse(config);
  if (cfg_tags) {
    if (grapes_config_value_int(cfg_tags, "flow_id", &ctx->flow_id)) {
        fprintf(stderr, "Flow id set to %i\n", ctx->flow_id);
    }
    free(cfg_tags);
  }
  else
    ctx->flow_id=0;

  *period = 40000;
  return ctx;
}

static void close(struct chunkiser_ctx *s)
{
  free(s);
}

static uint8_t *chunkise(struct chunkiser_ctx *s, chunkid_t id, chunksize_t *size, uint64_t *ts, void **attr, chunksize_t *attr_size, flowid_t *flow_id)
{
  sprintf(s->buff, "Chunk %d", id);
  *ts = 40 * id * 1000;
  *size = strlen(s->buff);
  *flow_id = s->flow_id;
  return strdup(s->buff);
}

struct chunkiser_iface in_dummy = {
  .open = open,
  .close = close,
  .chunkise = chunkise,
};
