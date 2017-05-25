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

#ifndef PJLIB_RTP
#ifndef RTP_RTCP_
#define RTP_RTCP_

#pragma pack(push)
#pragma pack(1)

#define RTP_RTCP_SUCCESS  (0)
#define RTP_RTCP_FAIL     (1)

#define RTP_VERSION       (2)

#define RTCP_SR           (200)

/*
*
*    The RTP header has the following format:
*
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |V=2|P|X|  CC   |M|     PT      |       sequence number         |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |                           timestamp                           |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
* |           synchronization source (SSRC) identifier            |
* +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
* |            contributing source (CSRC) identifiers             |
* |                             ....                              |
* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*
* RTP data header
*/
struct rtp_hdr {
  uint8_t   ver_p_x_cc;
  uint8_t   m_pt;
  uint16_t  seq;
  uint32_t  ts;
  uint32_t  ssrc;
};
#define RTP_HDR_SIZE  (sizeof(struct rtp_hdr))

#define RTP_HDR_VER_MASK  (0xC0)
#define RTP_HDR_P_MASK    (0x20)
#define RTP_HDR_X_MASK    (0x10)
#define RTP_HDR_CC_MASK   (0x0F)
#define RTP_HDR_M_MASK    (0x80)
#define RTP_HDR_PT_MASK   (0x7F)

#define RTP_HDR_VER_OFFSET  (6)
#define RTP_HDR_P_OFFSET    (5)
#define RTP_HDR_X_OFFSET    (4)
#define RTP_HDR_CC_OFFSET   (0)
#define RTP_HDR_M_OFFSET    (7)
#define RTP_HDR_PT_OFFSET   (0)

/* The parameter of the following macros must be a pointer to struct rtp_hdr */
#define RTP_HDR_VER(hdr)    ((((hdr)->ver_p_x_cc) &   \
                                RTP_HDR_VER_MASK) >>  \
                                RTP_HDR_VER_OFFSET)

#define RTP_HDR_P(hdr)      ((((hdr)->ver_p_x_cc) &   \
                                RTP_HDR_P_MASK) >>    \
                                RTP_HDR_P_OFFSET)

#define RTP_HDR_X(hdr)      ((((hdr)->ver_p_x_cc) &   \
                                RTP_HDR_X_MASK) >>    \
                                RTP_HDR_X_OFFSET)

#define RTP_HDR_CC(hdr)     ((((hdr)->ver_p_x_cc) &   \
                                RTP_HDR_CC_MASK) >>   \
                                RTP_HDR_CC_OFFSET)

#define RTP_HDR_M(hdr)      ((((hdr)->m_pt) &         \
                                RTP_HDR_M_MASK) >>    \
                                RTP_HDR_M_OFFSET)

#define RTP_HDR_PT(hdr)      ((((hdr)->m_pt) &        \
                                RTP_HDR_PT_MASK) >>   \
                                RTP_HDR_PT_OFFSET)

/*
* RTCP common header
*
*   0                   1                   2                   3
*   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |V=2|P|    RC   |   PT=SR=200   |             length            | header
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |                    SSRC Identification                        |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*
*/

struct rtcp_cmn_hdr {
  uint8_t   ver_p_rc;
  uint8_t   pt;
  uint16_t  length;
  uint32_t  ssrc;
};
#define RTCP_CMN_HDR_SIZE  (sizeof(struct rtcp_cmn_hdr))

#define RTCP_CMN_HDR_VER_MASK   (0xC0)
#define RTCP_CMN_HDR_P_MASK     (0x20)
#define RTCP_CMN_HDR_RC_MASK    (0x1F)

#define RTCP_CMN_HDR_VER_OFFSET   (6)
#define RTCP_CMN_HDR_P_OFFSET     (5)
#define RTCP_CMN_HDR_RC_OFFSET    (0)

/* The parameter of the following macros must be a pointer to struct rtcp_cmn_hdr */
#define RTCP_HDR_VER(hdr)     ((((hdr)->ver_p_rc) &   \
                                  RTCP_CMN_HDR_VER_MASK) >>  \
                                  RTCP_CMN_HDR_VER_OFFSET)

#define RTCP_HDR_P(hdr)       ((((hdr)->ver_p_rc) &   \
                                  RTCP_CMN_HDR_P_MASK) >>  \
                                  RTCP_CMN_HDR_P_OFFSET)

#define RTCP_HDR_RC(hdr)      ((((hdr)->ver_p_rc) &   \
                                  RTCP_CMN_HDR_RC_MASK) >>  \
                                  RTCP_CMN_HDR_RC_OFFSET)

/*
* Sender Report block
*
*  0               1               2               3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |            NTP timestamp, most significant word NTS           |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |             NTP timestamp, least significant word             |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |                       RTP timestamp RTS                       |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |                   sender's packet count SPC                   |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |                    sender's octet count SOC                   |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
struct rtcp_sr {
  uint32_t  ntp_sec;
  uint32_t  ntp_frac;
  uint32_t  rtp_ts;
  uint32_t  sender_pcount;
  uint32_t  sender_bcount;
};
#define RTCP_SR_SIZE  (sizeof(struct rtcp_sr))

/*
*  0                   1                   2                   3
*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |      defined by profile       |           length              |
*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*  |                        header extension                       |
*  |                             ....                              |
*/
struct rtp_ext_hdr {
  uint16_t profile_data;
  uint16_t length;
};
#define RTP_EXT_HDR_SIZE  (sizeof(struct rtp_ext_hdr))

#pragma pack(pop)


int rtp_rtcp_decode_rtp(const void *pkt, int pkt_len,
                        const struct rtp_hdr **hdr,
                        const void **payload,
                        unsigned int *payloadlen);

#endif /* RTP_RTCP_ */
#endif /* PJLIB_RTP */
