/*
 *  Copyright (c) 2014 Davide Kirchner
 *
 *  This is free software; see gpl-3.0.txt
 */

/*
  This file contains utitilites that are shared between rtp chunkiser
  (input-stream-rtp.c) and dechunkiser (output-stream-rtp.c)
 */

#define RTP_STREAMS_NUM_MAX 10
#define RTP_UDP_PORTS_NUM_MAX (2 * RTP_STREAMS_NUM_MAX)

/*
  Given a config string that is either <port> or <port>:<port>,
  fills ports[0] (rtp port) and ports[1] (rtcp port) with integers.
  Returns 0 on success, nonzero on failure.
 */
static inline int port_pair_parse(const char *conf_str, int *ports) {
  char *ptr;
  int port;

  ptr = strchr(conf_str, ':');
  if (ptr == NULL) {
    if (sscanf(conf_str, "%d", &port) == EOF) {
      return 1;
    }
    //fprintf(stderr, "  Parsed single port: %u\n", port);
    // pedantic compliant with RFC 5330, section 11:
    if (port % 2 == 0) {  // even port: this is RTP, the next is RTCP
      ports[0] = port;
      ports[1] = port + 1;
    }
    else {  // odd port: this is RTCP, previous is RTP
      ports[1] = port;
      ports[0] = port - 1;
    }
  }
  else {
    if (sscanf(conf_str, "%d:%d", &ports[0], &ports[1]) == EOF) {
      return 1;
    }
    //fprintf(stderr, "  Parsed double ports %u:%u\n", ports[0], ports[1]);
  }

  return 0;
}

/*
  Parses ports configuration: fills ports array and returns the number
  of ports found. If any error occurrs, returns 0 (note that if no
  ports are specified, it IS an error).
  If `video_stream_id` is not NULL, upon return it will contain the index
  of the video stream.
  In case an error occurrs, 0 is returned and `error_str` pointed to a message
 */
static inline int rtp_ports_parse(const struct tag *cfg_tags,
                                  int ports[RTP_UDP_PORTS_NUM_MAX + 1],
                                  int *video_stream_id,
                                  const char **error_str) {
  const char *val;
  int i = 0;
  int j;
  const char *port_pair_failed = \
    "Wrong format for port input: should be <port> or <rtp_port>:<rtcp_port>";

  /* Ports defined with explicit stream numbers */
  for (j = 0; j < RTP_STREAMS_NUM_MAX; j++) {
    char tag[8];

    sprintf(tag, "stream%d", j);
    val = grapes_config_value_str(cfg_tags, tag);
    if (val != NULL) {
      if (port_pair_parse(val, &ports[i]) != 0) {
        *error_str = port_pair_failed;
        return 0;
      }
      i += 2;
      if (video_stream_id != NULL) {
        *video_stream_id = 0;
      }
    }
  }
  /* Ports defined with "audio", "video" */
  if (i == 0) {
    val = grapes_config_value_str(cfg_tags, "audio");
    if (val != NULL) {
      if (port_pair_parse(val, &ports[i]) != 0) {
        *error_str = port_pair_failed;
        return 0;
      }
      i += 2;
    }
    val = grapes_config_value_str(cfg_tags, "video");
    if (val != NULL) {
      if (port_pair_parse(val, &ports[i]) != 0) {
        *error_str = port_pair_failed;
        return 0;
      }
      if (video_stream_id != NULL) {
        *video_stream_id = i / 2;
      }
      i += 2;
    }
  }
  /* Ports defined with "base" */
  if (i == 0) {
    if (grapes_config_value_int(cfg_tags, "base", &ports[0])) {
      ports[1] = ports[0] + 1;
      ports[2] = ports[1] + 1;
      ports[3] = ports[2] + 1;
      if (video_stream_id != NULL) {
        *video_stream_id = 1;
      }
      i += 4;
    }
  }
  // No port can be negative
  for (j = 0; j < i; j ++) {
    if (ports[j] < 0) {
      //printf_log(ctx, 0, "Negative ports (like %i) are not allowed", ports[j]);
      *error_str = "Negative ports not allowed";
      return 0;
    }
  }

  if (i == 0) {
    *error_str = "No listening port specified";
    return 0;
  }

  return i;
}
