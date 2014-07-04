#ifndef __FRAGMENTER_H__
#define __FRAGMENTER_H__

#include<list.h>
#include <stdint.h>
#include <sys/socket.h>

#define DEFAULT_FRAG_SIZE 100
#define FRAG_VOID 0
#define FRAG_DATA 1
#define FRAG_REQUEST 2
#define FRAG_EXPIRED 3


/*implementation dependent fragmenter handle*/
struct fragmenter;

struct fragmenter *fragmenter_init();

void fragmenter_destroy(struct fragmenter * frag);

/*outgoing part*/

int32_t fragmenter_add_msg(struct fragmenter *frag,const uint8_t * buffer_ptr,const int buffer_size,const size_t frag_size);
/*split buffer in fragments and return the message_id*/

uint8_t fragmenter_msg_frags(const struct fragmenter *frag,const uint16_t msg_id);
/*get the number of fragments of a msg*/

struct msghdr * fragmenter_get_frag(const struct fragmenter *frag,const uint16_t msg_id,const uint8_t frag_id);
/*return the specified fragment*/


/*incoming part*/
int fragmenter_frag_shrink(struct msghdr * frag_msg,const size_t data_size);

int32_t fragmenter_add_frag(struct fragmenter *frag,const struct msghdr * fragment);

uint8_t fragmenter_frag_init(struct msghdr * frag_msg,const uint16_t msg_id,const uint8_t frags_num ,const uint8_t frag_id,const uint8_t* data_ptr,const int data_size,const uint8_t type);

void fragmenter_frag_deinit(struct msghdr * frag_msg);

int fragmenter_pop_msg(struct fragmenter * frag,struct sockaddr_storage * msg_name, uint8_t * buffer_ptr,const int buffer_size);

int32_t fragmenter_frag_msgid(const struct msghdr * frag_msg);

int16_t fragmenter_frag_id(const struct msghdr * frag_msg);

int16_t fragmenter_frag_type(const struct msghdr * frag_msg);

struct msghdr * fragmenter_frag_requests(const struct fragmenter *frag,uint32_t * req_num);

void fragmenter_msg_remove(struct fragmenter *frag,const uint16_t msg_id);

uint8_t fragmenter_msg_exists(const struct fragmenter *frag,const uint16_t msg_id);
#endif
