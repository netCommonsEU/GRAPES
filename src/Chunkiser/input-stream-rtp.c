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
// #DEFINE NDEBUG
#include <assert.h>

#pragma message "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv"

#pragma GCC diagnostic ignored "-Wcast-qual"  // Ignore warnings from pj libs
#include <pjlib.h>
#include <pjmedia.h>
#pragma GCC diagnostic pop  // Restore warnings to command line options

#ifndef RTP_DEBUG
#define NDEBUG
#endif
#include <assert.h>

#include "int_coding.h"
#include "payload.h"
#include "config.h"
#include "chunkiser_iface.h"
#include "stream-rtp.h"

#define UDP_MAX_SIZE 65536   // 2^16
//#define RTP_DEFAULT_CHUNK_SIZE ((UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE) * 50)
#define RTP_DEFAULT_CHUNK_SIZE 20


struct rtp_stream {
  struct pjmedia_rtp_session rtp;
  struct pjmedia_rtcp_session rtcp;
};

struct chunkiser_ctx {
  // fixed context (set at opening time)
  int id;                 // TODO:  What is this for?? Is it needed?
  uint64_t start_time;    // TODO:  What is this for?? Is it needed?
  int every;              // ?? Not sure if this is really needed and how to use it
  int max_size;           // max `buff` size
  int video_stream_id;
  int rfc3551;
  int verbosity;
  int fds[RTP_UDP_PORTS_NUM_MAX + 1];
  int fds_len;  // even if "-1"-terminated, save length to make things easier
  struct rtp_stream streams[RTP_STREAMS_NUM_MAX];  // its len is fds_len/2
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
    if (loglevel == 0) {
      fflush(s);
    }
#ifdef DEBUG
    fflush(s);
#endif
  }
}


static int input_get_udp(uint8_t *data, int fd) {
  ssize_t msglen;

  msglen = recv(fd, data, UDP_MAX_SIZE, 0);
  if (msglen <= 0) {
    return 0;
  }

  return msglen;
}

static int listen_udp(const struct chunkiser_ctx *ctx, int port) {
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
  Return 0 on success, nonzero on failure.
 */
static int conf_parse(struct chunkiser_ctx *ctx, const char *config) {
  int ports[RTP_UDP_PORTS_NUM_MAX + 1];
  struct tag *cfg_tags;
  int i;
  const char *error_str = NULL;
  int chunk_size;

  /* Default context values */
  ctx->video_stream_id = -2;
  ctx->rfc3551 = 0;
  ctx->verbosity = 1;
  chunk_size = RTP_DEFAULT_CHUNK_SIZE;
  ctx->max_size = chunk_size + UDP_MAX_SIZE;
  for (i=0; i<RTP_UDP_PORTS_NUM_MAX + 1; i++) {
    ports[i] = -1;
  }

  /* Parse options */
  cfg_tags = config_parse(config);
  if (cfg_tags) {
    config_value_int(cfg_tags, "verbosity", &(ctx->verbosity));
    printf_log(ctx, 2, "Verbosity set to %i", ctx->verbosity);

    config_value_int(cfg_tags, "rfc3551", &(ctx->rfc3551));
    printf_log(ctx, 2, "%ssing RFC 3551",
               (ctx->rfc3551 ? "U" : "Not u"));
    
    if (config_value_int(cfg_tags, "chunk-size", &chunk_size)) {
      ctx->max_size = chunk_size + UDP_MAX_SIZE;
    } 
    printf_log(ctx, 2, "Chunk size (in bytes) is %d", chunk_size);
    printf_log(ctx, 2, "Maximum chunk size (in bytes) is thus %d", ctx->max_size);
    
    ctx->fds_len =
      rtp_ports_parse(cfg_tags, ports, &(ctx->video_stream_id), &error_str);

    if (ctx->rfc3551) {
      printf_log(ctx, 2, "The video stream for RFC 3551 is the one on ports %d:%d",
                 ports[ctx->video_stream_id], ports[ctx->video_stream_id+1]);
    }
  }
  free(cfg_tags);

  if (ctx->fds_len == 0) {
    printf_log(ctx, 0, error_str);
    return 1;
  }

  /* Open ports */
  for (i = 0; ports[i] >= 0; i++) {
    ctx->fds[i] = listen_udp(ctx, ports[i]);
    if (ctx->fds[i] < 0) {
      printf_log(ctx, 0, "Cannot open port %d", ports[i]);
      for (; i>=0 ; i--) {
        close(ctx->fds[i]);
      }
      return 2;
    }
  }
  ctx->fds[i] = -1;
  if (i != ctx->fds_len) {
    printf_log(ctx, 0, "Something very wrong happended.");
    return 3;
  }

  return 0;
}

static struct chunkiser_ctx *rtp_open(const char *fname, int *period, const char *config) {
  struct chunkiser_ctx *res;
  struct timeval tv;
  int i;

  pj_init();

  res = malloc(sizeof(struct chunkiser_ctx));
  if (res == NULL) {
    return NULL;
  }

  if(conf_parse(res, config) != 0) {
    printf_log(res, 0, "Error while parsing input parameters.");
    free(res);
    return NULL;
  }
  printf_log(res, 2, "Parameter parsing was successful.");  

  {
    struct pjmedia_rtp_session_setting rtp_s;
    struct pjmedia_rtcp_session_setting rtcp_s;
    rtp_s.flags = 0;
    pjmedia_rtcp_session_setting_default(&rtcp_s);
    rtcp_s.clock_rate = 1;  // Just to avoid Floating point exception
    for (i=0; i<res->fds_len/2; i++) {
      if (pjmedia_rtp_session_init2(&res->streams[i].rtp, rtp_s) != PJ_SUCCESS) {
        printf_log(res, 0, "Error initialising pjmedia RTP session");
        free(res);
        return NULL;
      }
      pjmedia_rtcp_init2(&res->streams[i].rtcp, &rtcp_s);
    }
  }

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

static void rtp_close(struct chunkiser_ctx  *ctx) {
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
static uint8_t *rtp_chunkise(struct chunkiser_ctx *ctx, int id, int *size, uint64_t *ts) {
  int j;
  int status = 0;  // 0: Go on; 1: ready to send; -1: error
  uint8_t *res;

  // Allocate new buffer if needed
  if (ctx->buff == NULL) {
    ctx->buff = malloc(ctx->max_size);
    if (ctx->buff != NULL) {
      ctx->size = RTP_PAYLOAD_FIXED_HEADER_SIZE;
      rtp_payload_header_init(ctx->buff);
    }
    else {
      printf_log(ctx, 0, "Could not alloccate chunk buffer.");
      *size = -1;
      status = -1;
    }
  }
  // Check open ports for incoming UDP packets in a round-robin
  for (j = 0; j < ctx->fds_len && status == 0; j++) {
    int i = (ctx->next_fd + j) % ctx->fds_len;
    int new_pkt_size;
    uint8_t *new_pkt_start =
      ctx->buff + ctx->size + RTP_PAYLOAD_PER_PKT_HEADER_SIZE;

    assert((ctx->max_size - ctx->size)
           >= (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE));

    new_pkt_size = input_get_udp(new_pkt_start, ctx->fds[i]);
    if (new_pkt_size) {
      printf_log(ctx, 2, "Got UDP message of size %d from port id #%d",
                 new_pkt_size, i);
      if (i % 2 == 0) {  // RTP packet
        const struct pjmedia_rtp_hdr *rtp_h;
        const void *rtp_p;
        int rtp_p_len;
        if (pjmedia_rtp_decode_rtp(&ctx->streams[i/2].rtp,
                                   new_pkt_start, new_pkt_size,
                                   &rtp_h, &rtp_p, &rtp_p_len)
            == PJ_SUCCESS) {
	  pjmedia_rtcp_rx_rtp(&ctx->streams[i/2].rtcp, ntohs(rtp_h->seq),
	                      ntohl(rtp_h->ts), rtp_p_len);
          pjmedia_rtp_session_update(&ctx->streams[i/2].rtp, rtp_h, NULL);
          printf_log(ctx, 2, "  -> valid RTP packet from ssrc:%u with "
                     "type:%u, marker:%u, seq:%u, timestamp:%u",
                     ntohl(rtp_h->ssrc), rtp_h->pt, rtp_h->m, ntohs(rtp_h->seq),
                     ntohl(rtp_h->ts));
        }
        else {
          printf_log(ctx, 1, "Got invalid RTP packet: skipping.");
          // TODO: flag for allowing forwarding non-RTP packets?
          continue;
        }
      }
      else {  // RTCP packet
        printf_log(ctx, 2, "  -> RTCP packet (is it valid? who knows)");
        pjmedia_rtcp_rx_rtcp(&ctx->streams[i/2].rtcp,
                             new_pkt_start, new_pkt_size);
      }
      rtp_payload_per_pkt_header_set(ctx->buff, ctx->size, new_pkt_size, i);
      ctx->size += new_pkt_size + RTP_PAYLOAD_PER_PKT_HEADER_SIZE;

      if ((ctx->max_size - ctx->size)
          < (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE)
      ) {  // Not enough space left in buffer: send chunk
        printf_log(ctx, 2, "Buffer size reached: (%d over %d - max %d)",
                   ctx->size, ctx->max_size - UDP_MAX_SIZE, ctx->max_size);
        status = 1;
        ctx->next_fd = i;
      }
    }
  }
  // inspect ntp time, for debugging purposes
  for (j = 0; j < ctx->fds_len/2; j++) {
    struct pjmedia_rtcp_ntp_rec ntp;
    pjmedia_rtcp_get_ntp_time(&ctx->streams[j].rtcp, &ntp);
    printf_log(ctx, 2, "NTP time for stream %d: Hi: %u Lo: %u", j, ntp.hi, ntp.lo);
  }
  printf_log(ctx, 2, ".");

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

#pragma message "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
