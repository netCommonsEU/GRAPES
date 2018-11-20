/*
 *  Copyright (c) 2010 Csaba Kiraly
 *  Copyright (c) 2014 Davide Kirchner
 *  Copyright (c) 2018 Massimo Girondi
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
  int verbosity;
  int *flows; //known flows
  int flows_number;
  int base_port; //First port to be opened
  int *ports; //Opened ports (4 for each flow)
};

/* Define a printf-like function for logging */
static void printf_log(const struct dechunkiser_ctx *ctx, int loglevel,
                       const char *fmt, ...) {
  va_list args;
  FILE* s = stderr;
  if (loglevel <= ctx->verbosity && strlen(fmt)) {
    fprintf(s, "RTP Multiflow De-chunkiser: ");
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
  int port;
  struct tag *cfg_tags;

  // defaults
  ctx->flows_number=0;
  ctx->flows=NULL;
  ctx->ports=NULL;
  ctx->verbosity = 1;
  sprintf(ctx->ip, "127.0.0.1");


  if (!config) {
    printf_log(ctx, 0, "No configuration specified");
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

    if(grapes_config_value_int(cfg_tags, "base_out", &port))
    {
      if(port < 0){
        printf_log(ctx, 0, "Port cannot be negative!");
        return 2;
      }
      else{
        ctx->base_port = port; //Ports are dinamically opened
      }
    }
    else{
        printf_log(ctx, 0, "No output port specified");
        return 2;
    }

   }
   free(cfg_tags);

  printf_log(ctx, 1, " Configured base output port: %i", ctx->base_port);
  printf_log(ctx, 1, " You can receive up to %i different flows", RTP_UDP_PORTS_NUM_MAX/4);

  return 0;
}



static int open_ports(struct dechunkiser_ctx *ctx,int flow)

/*
  Function to add a new flow (if there is space) to the flows array,
  Adding also the ports to ports array
  Return:
  -1 -> Problem opening ports or allocating memory
  >=0 -> Successfully opened ports, the index of the flow in flows array
*/
{
  int res = -1;
  int i,j;
  int *new_ports;
  int *new_flows;

  new_flows=malloc(sizeof(int) * (ctx->flows_number+1));
  new_ports=malloc(sizeof(int) * ((ctx->flows_number+1) * 4 ));

  if(!new_flows || !new_ports)
  {
    free(new_flows);
    free(new_ports);

    printf_log(ctx, 0, " New flow found! flow_id is %i, but there are been problem allocating memory!", flow);
    res=-1;
  }
  else
  {
    if(ctx->flows)
    {
      //memcpy(new_flows,ctx->flows,sizeof(int) * ctx->flows_number);
      //memcpy(new_ports,ctx->ports,sizeof(int) * ctx->flows_number);
      for(i=0;i<ctx->flows_number;i++)
        new_flows[i]=ctx->flows[i];
      for(i=0;i<ctx->flows_number*4;i++)
        new_ports[i]=ctx->ports[i];

      free(ctx->flows);
      free(ctx->ports);
    }
    ctx->flows=new_flows;
    ctx->ports=new_ports;

    ctx->flows[ctx->flows_number]=flow;
    i=ctx->flows_number*4;
    for(j = 0; j < 4; j++)
      ctx->ports[i+j] = ctx->base_port + (j+i);

    res=ctx->flows_number;
    ctx->flows_number+=1;

#ifdef DEBUG
    printf_log(ctx,1,"[PORT OPENING]: flow %i, assigned index %i\n\t\tOpened ports:",flow,ctx->flows_number);
    for(j = 0; j < 4; j++)
      printf_log(ctx,1,"%i, ",ctx->ports[i+j]);
    printf_log(ctx,1,"\n\t\tKnown flows: ");
    for(j = 0; j < ctx->flows_number; j++)
      printf_log(ctx,1,"%i, ",ctx->flows[j]);
    printf_log(ctx,1,"\n");
#endif


    printf_log(ctx, 1, " New flow found! flow_id is %i and assigned ports are [%i - %i]", flow, ctx->ports[i],ctx->ports[i+3]);
    }

  return res;
}

static int is_flow_known(struct dechunkiser_ctx *ctx,int flow)
/*
Return the index in flowx array if the flow is already known, -1 if not
*/
{
  int res = -1;
  int i;
  for(i = 0; i <ctx->flows_number;i++)
    if(ctx->flows[i]==flow)
      return i;
  return res;
}


static struct dechunkiser_ctx *rtp_multi_open_out(const char *fname, const char *config) {
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


static void rtp_multi_write(struct dechunkiser_ctx *ctx, chunkid_t id, uint8_t *data, chunksize_t size, flowid_t flow_id) {
  int i;
  uint8_t* data_end = data + size;
  printf_log(ctx, 2, "Got chunk of size %i belonging to flow #%i", size, flow_id);
  while (data < data_end) {
    // NOTE: `stream` here is the index in the ports array.
    int stream, psize;

    rtp_payload_per_pkt_header_parse(data, &psize, &stream);
    data += RTP_PAYLOAD_PER_PKT_HEADER_SIZE;

    i = is_flow_known(ctx,flow_id);
    if(i < 0)
    {
      i = open_ports(ctx, flow_id);
      if(i<0)
        return;
    }
    else{
      printf_log(ctx, 3, " I already know the flow_id %i!", flow_id);
    }
    
    printf_log(ctx, 3, " The port index was %i, now it's %i",stream, (stream+4*i));

    stream += 4*i;

    if (stream > ctx->flows_number*4) {
      printf_log(ctx, 1, "Received Chunk with bad stream %d > %d",
                 stream, ctx->flows_number*4);
      return;
    }

    printf_log(ctx, 2,
               "sending packet of size %i from port id #%i to port %i",
               psize, stream, ctx->ports[stream]);
    packet_write(ctx->outfd, ctx->ip, ctx->ports[stream], data, psize);
    data += psize;
  }
}

static void rtp_multi_close(struct dechunkiser_ctx *ctx) {
  close(ctx->outfd);
  if(ctx->flows)
    free(ctx->flows);
  if(ctx->ports)
    free(ctx->ports);

  free(ctx);
}

struct dechunkiser_iface out_rtp_multi = {
  .open = rtp_multi_open_out,
  .write = rtp_multi_write,
  .close = rtp_multi_close,
};
