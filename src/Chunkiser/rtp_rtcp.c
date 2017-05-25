/*
  Structures and support functions for RTP/RTCP protocols headers manipulation.

  Copyright (C) 2016 Nicolo' Facchi <nicolo.facchi@gmail.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <arpa/inet.h>

#include "rtp_rtcp.h"

int rtp_rtcp_decode_rtp(const void *pkt, int pkt_len,
                        const struct rtp_hdr **hdr,
                        const void **payload,
                        unsigned int *payloadlen)
{
  int offset;

  *hdr = (const struct rtp_hdr *)pkt;

  if (RTP_HDR_VER(*hdr) != RTP_VERSION)
    return RTP_RTCP_FAIL;

  offset = sizeof(struct rtp_hdr) + (RTP_HDR_CC(*hdr) * sizeof(uint32_t));

  if (RTP_HDR_X(*hdr)) {
    const struct rtp_ext_hdr *ext = (const struct rtp_ext_hdr *)
      (((const uint8_t *) pkt) + offset);
    offset += ((ntohs(ext->length) + 1) * sizeof(uint32_t));
  }

  if (offset > pkt_len)
    return RTP_RTCP_FAIL;

  *payload = ((const uint8_t *) pkt) + offset;
  *payloadlen = pkt_len - offset;

  if (RTP_HDR_P(*hdr) && *payloadlen > 0) {
    uint8_t pad_len;

    pad_len = ((const uint8_t *) (*payload))[*payloadlen - 1];

    if (pad_len <= *payloadlen)
      *payloadlen -= pad_len;
  }

  return RTP_RTCP_SUCCESS;
}
