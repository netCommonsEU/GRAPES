/*
 *  Copyright (c) 2014 Davide Kirchner
 *
 *  This is free software; see gpl-3.0.txt
 */
#include <sys/time.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#else
#include <winsock2.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>

#include "int_coding.h"
#include "payload.h"
#include "config.h"
#include "chunkiser_iface.h"
#include "stream-rtp.h"

#define UDP_MAX_SIZE 65536   // 2^16
//#define RTP_MAX_CHUNK_SIZE (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE) * 50
#define RTP_MAX_CHUNK_SIZE (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE) + 20

struct chunkiser_ctx {
  // fixed context (set at opening time)
  int fds[RTP_UDP_PORTS_NUM_MAX + 1];
  int fds_len;  // even if "-1"-terminated, save length to make things easier
  int id;                 // TODO:  What is this for?? Is it needed?
  uint64_t start_time;    // TODO:  What is this for?? Is it needed?
  int every;              // ?? Not sure if this is really needed and how to use it
  int max_size;           // max `buff` size
  int video_stream_id;
  int rfc3551;
  int verbosity;
  // running context (set at chunkising time)
  uint8_t *buff;          // chunk buffer
  int size;               // its current size
  int next_fd;            // next fd (index in fsd array) to be tried (in a round-robin)
  int counter;            // number of chinks sent
};


/* Define a printf-like function for logging */
static void printf_log(const struct chunkiser_ctx *ctx, int loglevel,
                       const char *fmt, ...) {
  va_list args;
  FILE* s = stderr;
  if (loglevel <= ctx->verbosity && strlen(fmt)) {
    fprintf(s, "RTP Chunkiser: ");
    if (loglevel == 0) {
      fprintf(s, "Error: ");
    }
    va_start(args, fmt);
    vfprintf(s, fmt, args);
    va_end(args);
    fprintf(s, "\n");
    fflush(s);
  }
}


static int input_get_udp(uint8_t *data, int fd)
{
  ssize_t msglen;

  msglen = recv(fd, data, UDP_MAX_SIZE, 0);
  // TODO: how mush space is left in buffer??
  if (msglen <= 0) {
    return 0;
  }

  return msglen;
}

static int listen_udp(const struct chunkiser_ctx *ctx, int port)
{
  struct sockaddr_in servaddr;
  int r;
  int fd;

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd < 0) {
    return -1;
  }

#ifndef _WIN32
  fcntl(fd, F_SETFL, O_NONBLOCK);
#else
  {
    unsigned long nonblocking = 1;
    ioctlsocket(fd, FIONBIO, (unsigned long*) &nonblocking);
  }
#endif

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  r = bind(fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
  if (r < 0) {
    close(fd);

    return -1;
  }
  printf_log(ctx, 2, "  opened fd:%d for port:%d", fd, port);


  return fd;
}


/*
  Read config string `config`, updates `ctx` accordingly.
  Also open required UDP ports, so as to save their file descriptors
  in context.
  Return nonzero on success, 0 on failure.
 */
static int conf_parse(struct chunkiser_ctx *ctx, const char *config)
{
  int ports[RTP_UDP_PORTS_NUM_MAX + 1];
  int ports_len;
  struct tag *cfg_tags;
  int i;
  const char *error_str;

  /* Default context values */
  ctx->video_stream_id = -1;
  ctx->rfc3551 = 0;
  ctx->verbosity = 1;
  ctx->max_size = RTP_MAX_CHUNK_SIZE;

  /* Parse options */
  cfg_tags = config_parse(config);
  if (cfg_tags) {
    config_value_int(cfg_tags, "verbosity", &(ctx->verbosity));
    printf_log(ctx, 2, "Verbosity set to %i", ctx->verbosity);

    config_value_int(cfg_tags, "rfc3551", &(ctx->rfc3551));
    printf_log(ctx, 2, "%ssing RFC 3551",
               (ctx->rfc3551 ? "U" : "Not u"));
    
    // TODO: read max_chunk_size
    printf_log(ctx, 2, "Maximum chunk size (in bytes) is %i", ctx->max_size);

    
    ctx->fds_len =
      rtp_ports_parse(cfg_tags, ports, &(ctx->video_stream_id), &error_str);
  }
  free(cfg_tags);

  if (ctx->fds_len == 0) {
    printf_log(ctx, 0, error_str);
    return 0;
  }

  /* Open ports */
  for (i = 0; ports[i] >= 0; i++) {
    ctx->fds[i] = listen_udp(ctx, ports[i]);
    if (ctx->fds[i] < 0) {
      printf_log(ctx, 0, "Cannot open port %d", ports[i]);
      for (; i>=0 ; i--) {
        close(ctx->fds[i]);
      }
      return 0;
    }
  }
  ctx->fds[i] = -1;
  if (i != ctx->fds_len) {
    printf_log(ctx, 0, "Something very wrong happended.");
    return 0;
  }

  return 1;
}

static struct chunkiser_ctx *rtp_open(const char *fname, int *period, const char *config)
{
  struct chunkiser_ctx *res;
  struct timeval tv;

  res = malloc(sizeof(struct chunkiser_ctx));
  if (res == NULL) {
    return NULL;
  }

  if(conf_parse(res, config) == 0) {
    printf_log(res, 0, "Error while parsing input parameters.");
    free(res);
    return NULL;
  }
  printf_log(res, 2, "Parameter parsing was successful.");  
 

  gettimeofday(&tv, NULL);
  res->start_time = tv.tv_usec + tv.tv_sec * 1000000ULL;
  res->id = 1;

  res->every = 1;
  res->buff = NULL;
  res->size = 0;
  res->counter = 0;
  res->next_fd = 0;
  *period = 0;

  return res;
}

static void rtp_close(struct chunkiser_ctx  *ctx)
{
  int i;

  for (i = 0; ctx->fds[i] >= 0; i++) {
    close(ctx->fds[i]);
  }
  free(ctx);
}

/*
  Creates a chunk.  If the chunk is created, returns a pointer to an
  alloccated memory buffer to chunk content if the chunk was created.
  The caller should `free` it up.  In this case, size and ts are set
  to the chunk's size and timestamp.

  If no data is available, returns NULL and size=0

  In case of error, return NULL and size=-1
 */
static uint8_t *rtp_chunkise(struct chunkiser_ctx *ctx, int id, int *size, uint64_t *ts)
{
  int i, j;
  int status = 0;  // 0: Go on; 1: ready to send; -1: error
  uint8_t *res;

  if (ctx->buff == NULL) {
    ctx->buff = malloc(ctx->max_size);
    if (ctx->buff != NULL) {
      ctx->size = RTP_PAYLOAD_FIXED_HEADER_SIZE;
      rtp_payload_header_init(ctx->buff);
    }
    else {
      printf_log(ctx, 1, "Could not alloccate chunk buffer.");
      *size = -1;
      status = -1;
    }
  }
  for (j = 0; j < ctx->fds_len && status == 0; j++) {
    i = (ctx->next_fd + j) % ctx->fds_len;
    if ((ctx->max_size - ctx->size)                          // available buffer
          < (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE) // max pkt size
    ) {
      // Not enough space left in buffer: send chunk
      printf_log(ctx, 2,
                 "Sending chunk: almost maximum size reached (%i over %i)",
                 ctx->size, ctx->max_size);
      status = 1;
      ctx->next_fd = i;
    }
    else {
      int new_pkt_size;
      new_pkt_size = input_get_udp(
        ctx->buff + ctx->size + RTP_PAYLOAD_PER_PKT_HEADER_SIZE,
        ctx->fds[i]
      );
      if (new_pkt_size) {
        rtp_payload_per_pkt_header_set(ctx->buff, ctx->size, new_pkt_size, i);
        ctx->size += new_pkt_size + RTP_PAYLOAD_PER_PKT_HEADER_SIZE;
        printf_log(ctx, 2, "Got UDP message with size %i", new_pkt_size);
      }
    }
  }

  if (status > 0) {
    struct timeval now;
    res = ctx->buff;
    *size = ctx->size;
    gettimeofday(&now, NULL);
    *ts = now.tv_sec * 1000000ULL + now.tv_usec;
    ctx->counter++; 
    ctx->buff = NULL;
    ctx->size = 0;
    printf_log(ctx, 2, "Chunk created: size %i, timestamp %lli", *size, *ts);
  }
  else if (status == 0) {
    *size = 0;
    res = NULL;
  }
  else {
    *size = -1;
    res = NULL;
    printf_log(ctx, 0, "rtp_chunkise exiting");
  }

  return res;
}

const int *rtp_get_fds(const struct chunkiser_ctx *ctx) {
  return ctx->fds;
}

struct chunkiser_iface in_rtp = {
  .open = rtp_open,
  .close = rtp_close,
  .chunkise = rtp_chunkise,
  .get_fds = rtp_get_fds,
};
