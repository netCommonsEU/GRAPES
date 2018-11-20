#ifndef __DECHUNKISER_IFACE_H__
#define __DECHUNKISER_IFACE_H__

#include<chunk.h>

struct dechunkiser_ctx;

struct dechunkiser_iface {
  struct dechunkiser_ctx *(*open)(const char *fname, const char *config);
  void (*close)(struct dechunkiser_ctx *s);
  void (*write)(struct dechunkiser_ctx *o, chunkid_t id, uint8_t *data, chunksize_t size, flowid_t flow_id);
};


#endif
