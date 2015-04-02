/*
 *  Copyright (c) 2010 Csaba Kiraly
 *  Copyright (c) 2014 Davide Kirchner
 *
 *  This is free software; see gpl-3.0.txt
 */
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#include <winsock2.h>
#endif
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "int_coding.h"
#include "payload.h"
#include "grapes_config.h"
#include "dechunkiser_iface.h"
#include "stream-rtp.h"

#define IP_ADDR_LEN 16

#ifdef _WIN32
static int inet_aton(const char *cp, struct in_addr *addr) {
    if( cp==NULL || addr==NULL )
    {
        return(0);
    }

    addr->s_addr = inet_addr(cp);
    return (addr->s_addr == INADDR_NONE) ? 0 : 1;
}
#endif

struct dechunkiser_ctx {
  int outfd;
  char ip[IP_ADDR_LEN];
  int ports[RTP_UDP_PORTS_NUM_MAX];
  int ports_len;
  int verbosity;
};

/* Define a printf-like function for logging */
static void printf_log(const struct dechunkiser_ctx *ctx, int loglevel,
                       const char *fmt, ...) {
  va_list args;
  FILE* s = stderr;
  if (loglevel <= ctx->verbosity && strlen(fmt)) {
    fprintf(s, "RTP De-chunkiser: ");
    if (loglevel == 0) {
      fprintf(s, "Error: ");
    }
    va_start(args, fmt);
    vfprintf(s, fmt, args);
    va_end(args);
    fprintf(s, "\n");
    if (loglevel == 0) {
      fflush(s);
    }
#ifdef DEBUG
    fflush(s);
#endif
  }
}


/*
  Parses config and populates ctx accordingly.
  Returns 0 on success, nonzero on failure.
 */
static int conf_parse(struct dechunkiser_ctx *ctx, const char *config) {
  int j;
  struct tag *cfg_tags;
  const char *error_str;

  // defaults
  ctx->verbosity = 1;
  sprintf(ctx->ip, "127.0.0.1");
  ctx->ports_len = 0;
  for (j=0; j<RTP_UDP_PORTS_NUM_MAX; j++) {
    ctx->ports[j] = -1;
  }

  if (!config) {
    printf_log(ctx, 0, "No output ports specified");
    return 1;
  }

  cfg_tags = grapes_config_parse(config);
  if (cfg_tags) {
    const char *addr;

    grapes_config_value_int(cfg_tags, "verbosity", &(ctx->verbosity));
    printf_log(ctx, 2, "Verbosity set to %i", ctx->verbosity);

    addr = grapes_config_value_str(cfg_tags, "addr");
    if (addr && strlen(addr) < IP_ADDR_LEN) {
      sprintf(ctx->ip, "%s", addr);
    }
    printf_log(ctx, 1, "Destination IP address: %s", ctx->ip);

    ctx->ports_len =
      rtp_ports_parse(cfg_tags, ctx->ports, NULL, &error_str);
  }
  free(cfg_tags);

  if (ctx->ports_len == 0) {
    printf_log(ctx, 0, error_str);
    return 2;
  }

  for (j=0; j<ctx->ports_len; j++) {
    printf_log(ctx, 1, "  Configured output port: %i", ctx->ports[j]);
  }

  return 0;
}


static struct dechunkiser_ctx *rtp_open_out(const char *fname, const char *config) {
  struct dechunkiser_ctx *res;

  res = malloc(sizeof(struct dechunkiser_ctx));
  if (res == NULL) {
    return NULL;
  }

  if (conf_parse(res, config) != 0) {
    free(res);
    return NULL;
  }

  res->outfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (res->outfd < 0) {
    printf_log(res, 0, "Could not open output socket");
    free(res);
    return NULL;
  }

  return res;
}


static void packet_write(int fd, const char *ip, int port, uint8_t *data, int size) {
  struct sockaddr_in si_other;

  memset(&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(port);
  if (inet_aton(ip, &si_other.sin_addr) == 0) {
    fprintf(stderr, "RTP De-chunkiser: output socket: inet_aton() failed\n");
    return;
  }

  sendto(fd, data, size, 0, (struct sockaddr *)&si_other, sizeof(si_other));
}


static void rtp_write(struct dechunkiser_ctx *ctx, int id, uint8_t *data, int size) {
  uint8_t* data_end = data + size;
  printf_log(ctx, 2, "Got chunk of size %i", size);
  while (data < data_end) {
    // NOTE: `stream` here is the index in the ports array.
    int stream, psize;

    rtp_payload_per_pkt_header_parse(data, &psize, &stream);
    data += RTP_PAYLOAD_PER_PKT_HEADER_SIZE;
    if (stream > ctx->ports_len) {
      printf_log(ctx, 1, "Received Chunk with bad stream %d > %d",
                 stream, ctx->ports_len);
      return;
    }

    printf_log(ctx, 2,
               "sending packet of size %i from port id #%i to port %i",
               psize, stream, ctx->ports[stream]);
    packet_write(ctx->outfd, ctx->ip, ctx->ports[stream], data, psize);
    data += psize;
  }
}

static void rtp_close(struct dechunkiser_ctx *ctx) {
  close(ctx->outfd);
  free(ctx);
}

struct dechunkiser_iface out_rtp = {
  .open = rtp_open_out,
  .write = rtp_write,
  .close = rtp_close,
};
