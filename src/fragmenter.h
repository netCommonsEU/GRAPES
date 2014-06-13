#ifndef __FRAGMENTER_H__
#define __FRAGMENTER_H__

#include<list.h>
#include <stdint.h>

struct my_hdr_t {
  uint16_t message_id;
  uint8_t frag_seq;
  uint8_t frags_num;
} __attribute__((packed));


/*implementation dependent fragmenter handle*/
struct fragmenter;

struct fragmenter *fragmenter_init();

/*outgoing part*/

uint16_t fragmenter_add_msg(const struct fragmenter *frag,const uint8_t * buffer_ptr,const int buffer_size);
/*split buffer in fragments and return the message_id*/

uint8_t fragmenter_frags_num(const struct fragmenter *frag,const uint16_t msg_id);
/*get the number of fragments of a msg*/

struct msghder * fragmenter_get_frag(const struct fragmenter *frag,const uint16_t msg_id,const uint8_t frag_id);
/*return the specified fragment*/


/*incoming part*/
uint16_t fragmenter_add_frag(const struct fragmenter *frag,struct msghder * fragment);

uint16_t fragmenter_get_msg(const struct fragmenter * frag, uint8_t buffer_ptr,const int buffer_size);

uint16_t fragmenter_msgs_num(const struct fragmenter *frag);

#endif
