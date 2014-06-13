#include<fragmenter.h>
#include <stdint.h>
#include <malloc.h>

struct fragmenter{
	uint16_t last_generated_id;
};

struct fragmenter *fragmenter_init()
{
	return (struct fragmenter*)malloc(sizeof(struct fragmenter));
}

uint16_t fragmenter_add_msg(const struct fragmenter *frag,const uint8_t * buffer_ptr,const int buffer_size)
{
	return 0;
}

uint8_t fragmenter_frags_num(const struct fragmenter *frag,const uint16_t msg_id)
{
	return 0;
}

struct msghder * fragmenter_get_frag(const struct fragmenter *frag,const uint16_t msg_id,const uint8_t frag_id)
{
	return NULL;
}


uint16_t fragmenter_add_frag(const struct fragmenter *frag,struct msghder * fragment)
{
	return 0;
}

uint16_t fragmenter_get_msg(const struct fragmenter * frag, uint8_t buffer_ptr,const int buffer_size)
{
	return 0;
}

uint16_t fragmenter_msgs_num(const struct fragmenter *frag)
{
	return 0;
}
