#ifndef __FRAGMENTER_H__
#define __FRAGMENTER_H__

#include<list.h>
#include <stdint.h>

#define DEFAULT_FRAG_SIZE 100
#define FRAG_VOID 0
#define FRAG_DATA 1
#define FRAG_REQUEST 2
#define FRAG_EXPIRED 3

struct my_hdr_t {
  uint16_t message_id;
  uint8_t frag_seq;
  uint8_t frags_num;
	uint8_t frag_type;
} __attribute__((packed));


/*implementation dependent fragmenter handle*/
struct fragmenter;

struct fragmenter *fragmenter_init();

/*outgoing part*/

uint16_t fragmenter_add_msg(struct fragmenter *frag,const uint8_t * buffer_ptr,const int buffer_size,const size_t frag_size);
/*split buffer in fragments and return the message_id*/

uint8_t fragmenter_frags_num(const struct fragmenter *frag,const uint16_t msg_id);
/*get the number of fragments of a msg*/

struct msghdr * fragmenter_get_frag(const struct fragmenter *frag,const uint16_t msg_id,const uint8_t frag_id);
/*return the specified fragment*/


/*incoming part*/
int fragmenter_frag_shrink(struct msghdr * frag_msg,const int data_size);

uint16_t fragmenter_add_frag(struct fragmenter *frag,const struct msghdr * fragment);

uint8_t fragmenter_frag_init(struct msghdr * frag_msg,const uint16_t msg_id,const uint8_t frags_num ,const uint8_t frag_id,const uint8_t* data_ptr,const int data_size,const uint8_t type);

void fragmenter_frag_deinit(struct msghdr * frag_msg);

int fragmenter_pop_msg(struct fragmenter * frag, uint8_t * buffer_ptr,const int buffer_size);

uint16_t fragmenter_msgs_num(const struct fragmenter *frag);

#endif
