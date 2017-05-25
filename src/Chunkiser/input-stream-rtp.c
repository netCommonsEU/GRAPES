/*
 *  Copyright (c) 2010 Csaba Kiraly
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

#ifdef DEBUG
#pragma message "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv"
#pragma message "RTP Chunkiser compiling in debug mode"
#endif

#ifdef PJLIB_RTP
#pragma GCC diagnostic ignored "-Wcast-qual"  // Ignore warnings from pj libs
#include <pjlib.h>
#include <pjmedia.h>
#pragma GCC diagnostic pop  // Restore warnings to command line options
#else
#include "rtp_rtcp.h"
#endif

#ifndef DEBUG
#define NDEBUG
#endif
#include <assert.h>

#include "int_coding.h"
#include "payload.h"
#include "grapes_config.h"
#include "chunkiser_iface.h"
#include "stream-rtp.h"

// ntp timestamp management utilities
#define TS_SHIFT 32
#define TS_FRACT_MASK ((1ULL << TS_SHIFT) - 1)

#define UDP_MAX_SIZE 65536   // 2^16
//#define RTP_DEFAULT_CHUNK_SIZE 20
#define RTP_DEFAULT_CHUNK_SIZE 65536
#define RTP_DEFAULT_MAX_DELAY (1ULL << (TS_SHIFT-2))  // 250 ms

struct rtp_ntp_ts {
  // both in HOST byte order
  uint64_t ntp;
  uint32_t rtp;
};

struct rtp_stream {
#ifdef PJLIB_RTP
  struct pjmedia_rtp_session rtp;
  struct pjmedia_rtcp_session rtcp;
#endif
  struct rtp_ntp_ts tss[2];
  int last_updated_ts;  // index in tss
};

struct chunkiser_ctx {
  // fixed context (set at opening time)
  uint64_t start_time;    // TODO:  What is this for?? Is it needed?
                          // (it was there un UDP chunkiser)
  int max_size;           // max `buff` size
  uint64_t max_delay;     // max delay to accumulate in a chunk [ntp format]
  int video_stream_id;    // index in `streams`
  int rfc3551;
  int verbosity;
  int rtp_log;
  int fds[RTP_UDP_PORTS_NUM_MAX + 1];
  int fds_len;  // even if "-1"-terminated, save length to make things easier
  struct rtp_stream streams[RTP_STREAMS_NUM_MAX];  // its len is fds_len/2
  // running context (set at chunkising time)
  uint8_t *buff;          // chunk buffer
  int size;               // its current size
  int next_fd;            // next fd (index in fsd array) to be tried (in a round-robin)
  int counter;            // number of chunks sent
  uint64_t min_ntp_ts;    // ntp timestamp of first packet in chunk
  uint64_t max_ntp_ts;    // ntp timestamp of last packet in chunk
  int ntp_ts_status;      // known (1), yet unkwnown (0) or unknown (-1)
};

/* Holds relevant information extracted from each RTP packet */
struct rtp_info {
  uint16_t valid;
  uint16_t marker;
  uint64_t ntp_ts;
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


/* SUPPORT FUNCTIONS FOR TIMESTAMPS MANAGEMENT AND CONVERSIONS */

/* Multiplies 2 uint64_t, returning 0 and logging error on overflow */
static inline uint64_t mult_overflow(const struct chunkiser_ctx *ctx, uint64_t a, uint64_t b) {
  int n_a, n_b;
  uint64_t res;
  if (a == 0ULL || b == 0ULL) {
    // skip overflow ckecking (__builtin_clzll may fail)
    return 0ULL;
  }
  res = a * b;

  n_a = 64 - __builtin_clzll(a);
  n_b = 64 - __builtin_clzll(b);
  if (n_a + n_b > 64) {
    /* approximate check failed, proceed with exact check */
    if (res / a != b) {
      printf_log(ctx, 2, "Overflow during timestamp computation.");
      return 0ULL;
    }
  }
  return res;
}


/* Converts timestamps. If impossible, returns 0 */
static uint64_t rtptontp(const struct chunkiser_ctx *ctx,
                         const struct rtp_stream *stream, uint32_t rtp) {
  // latest rtp-ntp matching
  const struct rtp_ntp_ts *b = &stream->tss[stream->last_updated_ts];
  // second latest rtp-ntp matching
  const struct rtp_ntp_ts *a = &stream->tss[1 - stream->last_updated_ts];
  uint64_t tmp;

  if ((a->rtp == 0 && a->ntp == 0) || (b->rtp == 0 && b->ntp == 0)) {
    printf_log(ctx, 2, "NTP timestamp not (yet) computable.");
    return 0ULL;
  }
  else if (b->rtp - a->rtp == 0) {  // Will have to divide by this
    printf_log(ctx, 1,
               "Warning: there were two equal timestamps in the RTCP flow!");
    return 0ULL;
  }
  else {
    /*
         a         b    new
      ---|---------|-----*--> RTP timeline
      ---|---------|-----?--> NTP timeline
      a and b are known via RTCP. We assume (new _after_ a) and (b _after_ a)
      Note both timelines are circular
     */
    // Make sure the time interval (a -> b) is shorter than (b -> a)
    // (One of the two intervals will encompass a timer overflow).
    // Otherways something is probably very wrong.
    assert((b->rtp - a->rtp) < (a->rtp - b->rtp)); 
    // Similarly, with newly arrived and a
    assert((rtp - a->rtp) < (a->rtp - rtp));
    // Similarly with ntp
    assert((b->ntp - a->ntp) < (a->ntp - b->ntp));

    // return (rtp - a->rtp) * (b->ntp - a->ntp) / (b->rtp - a->rtp) + a->ntp
    // but handle multiplication overflow

    tmp = mult_overflow(ctx, (rtp - a->rtp), (b->ntp - a->ntp));
    if (tmp == 0) {
      return 0ULL;
    }
    return tmp / (b->rtp - a->rtp) + a->ntp;
  }
}


/* Returns min/max between 2 ntp timestamps, assuming inputs are
   "close enough"
 */
static uint64_t ts_min(uint64_t a, uint64_t b) {
  if (b - a < a - b) return a;
  else return b;
}
static uint64_t ts_max(uint64_t a, uint64_t b) {
  if (b - a < a - b) return b;
  else return a;
}


/* SUPPORT FUNCTIONS FOR RTP LIBRARY MANAGEMENT */

/* Initializes PJSIP. Returns 0 on success, nonzero on failure */
static int rtplib_init(struct chunkiser_ctx *ctx) {
  int i;
#ifdef PJLIB_RTP
  struct pjmedia_rtp_session_setting rtp_s;
  struct pjmedia_rtcp_session_setting rtcp_s;

  printf_log(ctx, 2, "PJLIB initialization");
  pj_init();

  rtp_s.flags = 0;
  pjmedia_rtcp_session_setting_default(&rtcp_s);
  rtcp_s.clock_rate = 1;  // Just to avoid Floating point exception
#else
  printf_log(ctx, 2, "RTP internal implementation initialized (PJSIP not used)");
#endif /* PJLIB_RTP */

  for (i=0; i<ctx->fds_len/2; i++) {
#ifdef PJLIB_RTP
    pjmedia_rtcp_init2(&ctx->streams[i].rtcp, &rtcp_s);
    if (pjmedia_rtp_session_init2(&ctx->streams[i].rtp, rtp_s) != PJ_SUCCESS) {
      printf_log(ctx, 0, "Error initialising pjmedia RTP session");
      return 1;
    }
#endif /* PJLIB_RTP */
    ctx->streams[i].last_updated_ts = 0;
    ctx->streams[i].tss[0].ntp = ctx->streams[i].tss[0].rtp = 0;
    ctx->streams[i].tss[1].ntp = ctx->streams[i].tss[1].rtp = 0;
  }
  return 0;
}


/* Inspects the given RTP packet and fills `info` accordingly. */
static void rtp_packet_received(struct chunkiser_ctx *ctx, int stream_id, uint8_t *pkt, int size, struct rtp_info *info) {
#ifdef PJLIB_RTP
  const struct pjmedia_rtp_hdr *rtp_h;
#else
  const struct rtp_hdr *rtp_h;
#endif
  const void *rtp_p;
  int rtp_p_len;
  struct rtp_stream *stream = &ctx->streams[stream_id];

#ifdef PJLIB_RTP
  if (pjmedia_rtp_decode_rtp(&stream->rtp,
                             pkt, size,
                             &rtp_h, &rtp_p, &rtp_p_len)
      == PJ_SUCCESS) {
    pjmedia_rtcp_rx_rtp(&stream->rtcp, ntohs(rtp_h->seq),
                        ntohl(rtp_h->ts), rtp_p_len);
    pjmedia_rtp_session_update(&stream->rtp, rtp_h, NULL);
    printf_log(ctx, 2, "  -> valid RTP packet from ssrc:%u with "
               "type:%u, marker:%u, seq:%u, timestamp:%u",
               ntohl(rtp_h->ssrc), rtp_h->pt, rtp_h->m, ntohs(rtp_h->seq),
               ntohl(rtp_h->ts));
#else
  if (rtp_rtcp_decode_rtp(pkt, size, &rtp_h, &rtp_p, &rtp_p_len) ==
      RTP_RTCP_SUCCESS) {
    printf_log(ctx, 2, "  -> valid RTP packet from ssrc:%u with "
               "type:%u, marker:%u, seq:%u, timestamp:%u",
               ntohl(rtp_h->ssrc), RTP_HDR_P(rtp_h), RTP_HDR_M(rtp_h),
               ntohs(rtp_h->seq), ntohl(rtp_h->ts));
#endif

    info->valid = 1;
#ifdef PJLIB_RTP
    info->marker = rtp_h->m;
#else
    info->marker = RTP_HDR_M(rtp_h);
#endif
    info->ntp_ts = rtptontp(ctx, stream, ntohl(rtp_h->ts));
  }
  else {
    printf_log(ctx, 1, "Warning: got invalid RTP packet (forwarding anyway).");
    info->valid = 0;
  }
}


/* Updates the context upon reception of RTCP packets. */
static void rtcp_packet_received(struct chunkiser_ctx *ctx, int stream_id, uint8_t *pkt, int size) {
  struct rtp_stream *stream = &ctx->streams[stream_id];

#ifdef PJLIB_RTP
  pjmedia_rtcp_rx_rtcp(&stream->rtcp, pkt, size);

  // Parse RTCP packet with low-level API.
  // Few lines taken from pjmedia/src/pjmedia/rtcp.c in pjproject
  {
    pj_uint8_t *p, *p_end;
    p = (pj_uint8_t*)pkt;
    p_end = p + size;
    while (p < p_end) {
      pjmedia_rtcp_common *common = (pjmedia_rtcp_common*)p;
      unsigned len = (pj_ntohs((pj_uint16_t)common->length)+1) * 4;
      if (common->pt == 200) {  // packet type 200 is Sender Report (SR)
        const pjmedia_rtcp_sr *sr =
          (pjmedia_rtcp_sr*) (((char*)pkt) + sizeof(pjmedia_rtcp_common));
        stream->last_updated_ts = (stream->last_updated_ts + 1) % 2;
        stream->tss[stream->last_updated_ts].rtp =
          ntohl(sr->rtp_ts);
        stream->tss[stream->last_updated_ts].ntp =
          (((uint64_t)ntohl(sr->ntp_sec)) << TS_SHIFT)
          + (((uint64_t)ntohl(sr->ntp_frac)) & TS_FRACT_MASK);
      }
      p += len;
    }
  }
#else
  uint8_t *p, *p_end;
  p = pkt;
  p_end = p + size;

  printf_log(ctx, 2, "  -> RTCP packet");

  while (p < p_end) {
    const struct rtcp_cmn_hdr *common = (const struct rtcp_cmn_hdr *) p;
    uint32_t len = (ntohs(common->length) + 1) * 4;

    if (common->pt == RTCP_SR) {
      const struct rtcp_sr *sr = (const struct rtcp_sr *)
        (p + sizeof(struct rtcp_cmn_hdr));
      stream->last_updated_ts = (stream->last_updated_ts + 1) % 2;
      stream->tss[stream->last_updated_ts].rtp =
        ntohl(sr->rtp_ts);
      stream->tss[stream->last_updated_ts].ntp =
        (((uint64_t)ntohl(sr->ntp_sec)) << TS_SHIFT)
        + (((uint64_t)ntohl(sr->ntp_frac)) & TS_FRACT_MASK);
    }
    p += len;
  }
#endif
  return;
}


/* SUPPORT FUNCTIONS FOR UDP SOCKETS MANAGEMENT */

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


/* SUPPORT FUNCTIONS FOR COMMAND-LINE CONFIGURATION */

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
  int max_delay_input;

  /* Default context values */
  ctx->video_stream_id = -2;
  ctx->rfc3551 = 0;
  ctx->verbosity = 1;
  ctx->rtp_log = 0;
  chunk_size = RTP_DEFAULT_CHUNK_SIZE;
  ctx->max_size = chunk_size + UDP_MAX_SIZE;
  ctx->max_delay = RTP_DEFAULT_MAX_DELAY;
  for (i=0; i<RTP_UDP_PORTS_NUM_MAX + 1; i++) {
    ports[i] = -1;
  }

  /* Parse options */
  cfg_tags = grapes_config_parse(config);
  if (cfg_tags) {
    grapes_config_value_int(cfg_tags, "verbosity", &(ctx->verbosity));
    printf_log(ctx, 2, "Verbosity set to %i", ctx->verbosity);

    grapes_config_value_int(cfg_tags, "rtp_log", &(ctx->rtp_log));
    if (ctx->rtp_log) {
        printf_log(ctx, 2, "Producing parsable rtp log. "
                   "TimeStamps are expressed in 2^-32 s.");
    }

    grapes_config_value_int(cfg_tags, "rfc3551", &(ctx->rfc3551));
    printf_log(ctx, 2, "%ssing RFC 3551",
               (ctx->rfc3551 ? "U" : "Not u"));

    if (grapes_config_value_int(cfg_tags, "chunk_size", &chunk_size)) {
      ctx->max_size = chunk_size + UDP_MAX_SIZE;
    }
    printf_log(ctx, 2, "Chunk size is %d bytes", chunk_size);
    printf_log(ctx, 2, "Maximum chunk size is thus %d bytes", ctx->max_size);

    if (grapes_config_value_int(cfg_tags, "max_delay_ms", &max_delay_input)) {
      ctx->max_delay = max_delay_input * (1ULL << TS_SHIFT) / 1000;
    }
    else if (grapes_config_value_int(cfg_tags, "max_delay_s", &max_delay_input)) {
      ctx->max_delay = max_delay_input * (1ULL << TS_SHIFT);
    }
    printf_log(ctx, 2, "Maximum delay set to %.0f ms.",
               ctx->max_delay * 1000.0 / (1ULL << TS_SHIFT));

    ctx->fds_len =
      rtp_ports_parse(cfg_tags, ports, &(ctx->video_stream_id), &error_str);

    if (ctx->rfc3551) {
      printf_log(ctx, 2,
                 "The video stream for RFC 3551 is the one on ports %d:%d",
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


/* ACTUAL "PUBLIC" FUNCTIONS, exposed via `struct chunkiser_iface in_rtp` */

static struct chunkiser_ctx *rtp_open(const char *fname, int *period, const char *config) {
  struct chunkiser_ctx *res;
  struct timeval tv;

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

  if (rtplib_init(res) != 0) {
    free(res);
    return NULL;
  }

  gettimeofday(&tv, NULL);
  res->start_time = tv.tv_usec + tv.tv_sec * 1000000ULL;

  res->buff = NULL;
  res->size = 0;
  res->counter = 0;
  res->next_fd = 0;
  res->ntp_ts_status = 0;
  *period = 0;

  return res;
}


static void rtp_close(struct chunkiser_ctx  *ctx) {
  int i;

  if (ctx->buff != NULL) {
    free(ctx->buff);
  }
  for (i = 0; ctx->fds[i] >= 0; i++) {
    close(ctx->fds[i]);
  }
  free(ctx);
}


/*
  Creates a chunk.  If the chunk is created successfully, returns a
  pointer to an alloccated memory buffer to chunk content.  The caller
  should `free` it up.  In this case, size and ts are set to the
  chunk's size and timestamp.

  If no data is available, returns NULL and size=0

  In case of error, returns NULL and size=-1
 */
static uint8_t *rtp_chunkise(struct chunkiser_ctx *ctx, int id, int *size, uint64_t *ts,
                                      void **attr, int *attr_size) {
  int status;  // -1: buffer full, send now
               //  0: Go on, do not send;
               //  1: send after loop;
               //  2: do one more round-robin loop now
  int j;
  uint8_t *res;

  // Allocate new buffer if needed
  if (ctx->buff == NULL) {
    ctx->buff = malloc(ctx->max_size);
    ctx->ntp_ts_status = 0;
    if (ctx->buff == NULL) {
      printf_log(ctx, 0, "Could not alloccate chunk buffer: exiting.");
      *size = -1;
      return NULL;
    }
  }
  do {
    status = 0;
    // Check open ports for incoming UDP packets in a round-robin
    for (j = 0; j < ctx->fds_len && status >= 0; j++) {
      int i = (ctx->next_fd + j) % ctx->fds_len;
      int new_pkt_size;
      uint8_t *new_pkt_start =
        ctx->buff + ctx->size + RTP_PAYLOAD_PER_PKT_HEADER_SIZE;
      struct rtp_info info;

      assert((ctx->max_size - ctx->size)
             >= (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE));

      new_pkt_size = input_get_udp(new_pkt_start, ctx->fds[i]);
      if (new_pkt_size) {
        printf_log(ctx, 2, "Got UDP message of size %d from port id #%d",
                   new_pkt_size, i);
        if (i % 2 == 0) {  // RTP packet
          rtp_packet_received(ctx, i/2, new_pkt_start, new_pkt_size, &info);
          if (info.valid) {
            printf_log(ctx, 2, "  packet has NTP timestamp (seconds) %llu",
                       info.ntp_ts >> TS_SHIFT);
            if (ctx->rtp_log) {
              fprintf(stderr, "[RTP_LOG] timestamp=%lu size=%d port_id=%d\n", info.ntp_ts, new_pkt_size, i);
            }
            // update chunk timestamp
            if (info.ntp_ts == 0ULL) {
              // packet with unknown ts, ignore all timestamps
              ctx->ntp_ts_status = -1;
            }
            if (ctx->ntp_ts_status >= 0) {
              switch (ctx->ntp_ts_status) {
              case 0:
                ctx->min_ntp_ts = info.ntp_ts;
                ctx->max_ntp_ts = info.ntp_ts;
                ctx->ntp_ts_status = 1;
                break;
              case 1:
                ctx->min_ntp_ts = ts_min(ctx->min_ntp_ts, info.ntp_ts);
                ctx->max_ntp_ts = ts_max(ctx->max_ntp_ts, info.ntp_ts);
                break;
              }
              if ((ctx->max_ntp_ts - ctx->min_ntp_ts) >= ctx->max_delay) {
                printf_log(ctx, 2, "  Max delay reached: %.0f over %.0f ms",
                           (ctx->max_ntp_ts - ctx->min_ntp_ts) * 1000.0 / (1ULL << TS_SHIFT),
                           ctx->max_delay * 1000.0 / (1ULL << TS_SHIFT));
                status = ((status > 1) ? status : 1); // status = max(status, 1)
              }
            }
            // Marker bit semantic for video stream in rfc3551
            if (ctx->rfc3551 && i/2 == ctx->video_stream_id && !info.marker) {
              printf_log(ctx, 2, "  Waiting for another part of this frame!");
              status = 2;
            }
          }
        }
        else {  // RTCP packet
          rtcp_packet_received(ctx, i/2, new_pkt_start, new_pkt_size);
        }
        // append packet to chunk
        rtp_payload_per_pkt_header_set(ctx->buff + ctx->size, new_pkt_size, i);
        ctx->size += new_pkt_size + RTP_PAYLOAD_PER_PKT_HEADER_SIZE;

        if ((ctx->max_size - ctx->size)
            < (UDP_MAX_SIZE + RTP_PAYLOAD_PER_PKT_HEADER_SIZE)
            ) {  // Not enough space left in buffer: send chunk
          printf_log(ctx, 2, "Buffer size reached: (%d over %d - max %d)",
                     ctx->size, ctx->max_size - UDP_MAX_SIZE, ctx->max_size);
          status = -1;
          ctx->next_fd = i;
        }
      }
    }
  } while (status >= 2);

  if (status == 0) {
    *size = 0;
    res = NULL;
  }
  else {
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


#ifdef DEBUG
#pragma message "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^"
#endif
