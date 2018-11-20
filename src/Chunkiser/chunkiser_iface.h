#ifndef __CHUNKISER_IFACE_H__
#define __CHUNKISER_IFACE_H__ 

#include<chunk.h>

struct chunkiser_ctx;

struct chunkiser_iface {
  struct chunkiser_ctx *(*open)(const char *fname, int *period, const char *config);
  void (*close)(struct chunkiser_ctx *s);
  uint8_t *(*chunkise)(struct chunkiser_ctx *s, chunkid_t id, chunksize_t *size, uint64_t *ts, void **attr, chunksize_t *attr_size, flowid_t *flow_id);
  const int *(*get_fds)(const struct chunkiser_ctx *s);
};


#endif
