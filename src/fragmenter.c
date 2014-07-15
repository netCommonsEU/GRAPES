#include<fragmenter.h>
#include <stdint.h>
#include <malloc.h>
#include <sys/socket.h>
#include <string.h>

#define MAX_MSG_NUM 31
#define MIN(A,B) ((A)<(B)?(A):(B))
#define MAX(A,B) ((A)>(B)?(A):(B))

struct fragmenter{
	uint16_t last_message_id;
	uint16_t queue_size;
	struct msghdr * queue[MAX_MSG_NUM];
};

struct my_hdr_t {
  uint16_t message_id;
  uint8_t frag_seq;
  uint8_t frags_num;
	uint8_t frag_type;
} __attribute__((packed));

struct fragmenter *fragmenter_init()
{
	uint16_t i;
	struct fragmenter * f = (struct fragmenter*)malloc(sizeof(struct fragmenter));
	if (f)
	{	
		f->last_message_id = 0;
		for (i=0;i<MAX_MSG_NUM;i++)
			f->queue[i] = NULL;
		return f;
	}
	else
		return NULL;
}

struct my_hdr_t * fragmenter_frag_header(const struct msghdr * frag_msg)
{
	if (frag_msg && frag_msg->msg_iovlen > 0)
		return ((struct my_hdr_t *) (frag_msg->msg_iov[0].iov_base));
	return NULL;
}
int32_t fragmenter_queue_pos2msg_id(const struct fragmenter * frag, uint16_t pos)
{
	struct my_hdr_t * hdr;
	if((frag->queue[pos%MAX_MSG_NUM]) != NULL)
	{
		hdr = fragmenter_frag_header(&(frag->queue[pos%MAX_MSG_NUM][0]));
		if (hdr != NULL)
		{
			return hdr->message_id;
		}
	}
	return -1; 
}

void fragmenter_destroy(struct fragmenter * frag)
{
	uint16_t i;
	for (i=0;i<MAX_MSG_NUM;i++)
		fragmenter_msg_remove(frag,fragmenter_queue_pos2msg_id(frag,i));
	free(frag);
	
}

uint8_t fragmenter_msg_exists(const struct fragmenter *frag,const uint16_t msg_id)
{
	uint8_t res = 0;
	struct my_hdr_t* hdr;
	if(frag && frag->queue[msg_id%MAX_MSG_NUM] != NULL)
	{
		hdr = fragmenter_frag_header(&(frag->queue[msg_id%MAX_MSG_NUM][0]));
		if (hdr && hdr->message_id == msg_id)
			res = 1;
	}
	return res;
}

uint8_t fragmenter_msg_init(struct fragmenter * frag, const uint16_t msg_id,const uint8_t frags_num)
{
	uint8_t i;
	if(!fragmenter_msg_exists(frag,msg_id))
	{
		frag->queue[msg_id%MAX_MSG_NUM] = (struct msghdr *)malloc(sizeof(struct msghdr)*frags_num);
		for(i=0;i<frags_num;i++)
			fragmenter_frag_init(&(frag->queue[msg_id%MAX_MSG_NUM][i]),msg_id,frags_num,i,NULL,0,FRAG_VOID);
		return 0;
	}
	return 1;
}

void fragmenter_msg_remove(struct fragmenter *frag,const uint16_t msg_id)
{
	uint8_t frags_num,i;

	if(frag)
	{
		if(fragmenter_msg_exists(frag,msg_id))
		{	
			frags_num = fragmenter_msg_frags(frag,msg_id);
			for(i=0;i<frags_num;i++)
				fragmenter_frag_deinit(&(frag->queue[(msg_id%MAX_MSG_NUM)][i]));

			free(frag->queue[msg_id%MAX_MSG_NUM]);
		}
		frag->queue[msg_id%MAX_MSG_NUM] = NULL;
	}
}

int32_t fragmenter_add_msg(struct fragmenter *frag,const uint8_t * buffer_ptr,const int buffer_size,const size_t frag_size)
{
	int32_t msg_id = -1;
	uint8_t frags_num,i;
	size_t frag_data;

	if (frag && buffer_size > 0 && buffer_ptr)
	{
		msg_id = (frag->last_message_id++);

		if (fragmenter_msg_exists(frag,msg_id))
			fragmenter_msg_remove(frag,msg_id);

		frag_data = frag_size>sizeof(struct my_hdr_t) ? frag_size-sizeof(struct my_hdr_t) : 1;
		frags_num = (buffer_size / frag_data)+((buffer_size % frag_data) ? 1 :0); 
		fragmenter_msg_init(frag,msg_id,frags_num);

		for(i=0;i<frags_num;i++)
			fragmenter_frag_init(&(frag->queue[msg_id%MAX_MSG_NUM][i]),msg_id,frags_num ,i,buffer_ptr+i*frag_data,MIN(frag_data,buffer_size-i*frag_data),FRAG_DATA);
	}
	return msg_id;
}

struct msghdr * fragmenter_get_frag(const struct fragmenter *frag,const uint16_t msg_id,const uint8_t frag_id)
{
	if(frag && fragmenter_msg_exists(frag,msg_id) && frag_id < fragmenter_msg_frags(frag,msg_id))
		return &(frag->queue[msg_id%MAX_MSG_NUM][frag_id]);

	return NULL;
}

uint32_t fragmenter_frag_msgname(const struct msghdr * frag_msg,struct sockaddr_storage * addr)
{
	memmove(addr,frag_msg->msg_name,frag_msg->msg_namelen); 
	return frag_msg->msg_namelen;
}	

void fragmenter_frag_deinit(struct msghdr * frag_msg)
	//free iov structures, i.e., header and data
{
	uint8_t i;
	if(frag_msg && frag_msg->msg_iov && frag_msg->msg_iovlen > 0)
	{	
		for (i=0;i<frag_msg->msg_iovlen;i++)
			if((frag_msg->msg_iov)[i].iov_base)
			{
				free((frag_msg->msg_iov)[i].iov_base);
				(frag_msg->msg_iov)[i].iov_base = NULL;
			}
		free(frag_msg->msg_iov);
		frag_msg->msg_iov = NULL;
	}
}

int fragmenter_frag_shrink(struct msghdr * frag_msg,const size_t frag_size)
	//inclusive of header size
{
	struct iovec * iov;
	int16_t data_size = frag_size-sizeof(struct my_hdr_t);

	if (frag_msg)
	{
		iov = frag_msg->msg_iov;	
		iov[1].iov_len = MAX(data_size,0);
		iov[1].iov_base = realloc(iov[1].iov_base,iov[1].iov_len);
		return iov[1].iov_len;
	}
	return -1;
}

uint8_t fragmenter_frag_init(struct msghdr * frag_msg,const uint16_t msg_id,const uint8_t frags_num ,const uint8_t frag_id,const uint8_t* data_ptr,const int data_size,const uint8_t type)
	/*returns 0 on success*/
{
		struct iovec * iov;
		if(frag_msg && frag_id < frags_num)
		{
			memset(frag_msg,0,sizeof(struct msghdr));
			iov = (struct iovec *) malloc(sizeof(struct iovec) * 2);

			iov[0].iov_len = sizeof(struct my_hdr_t);
			iov[0].iov_base = malloc(sizeof(struct my_hdr_t));
			((struct my_hdr_t *)iov[0].iov_base)->message_id = msg_id;
			((struct my_hdr_t *)iov[0].iov_base)->frags_num = frags_num;
			((struct my_hdr_t *)iov[0].iov_base)->frag_seq = frag_id;
			((struct my_hdr_t *)iov[0].iov_base)->frag_type = type;
			
			if (data_size > 0)
			{
				iov[1].iov_len = data_size;
				iov[1].iov_base = malloc(iov[1].iov_len);
				if(data_ptr)
					memmove(iov[1].iov_base,data_ptr,(size_t)iov[1].iov_len);
			}

			frag_msg->msg_iovlen = 2;
			frag_msg->msg_iov = iov;

			frag_msg->msg_namelen = sizeof(struct sockaddr_storage);
			frag_msg->msg_name = malloc(frag_msg->msg_namelen);
			memset(frag_msg->msg_name,0,frag_msg->msg_namelen);
		
			return 0;
		}
		return 1;
}

uint8_t fragmenter_msg_complete(const struct fragmenter * frag,uint16_t msg_id)
{
	uint8_t frags_num,complete=1,i;
	struct my_hdr_t * hdr;
	if(fragmenter_msg_exists(frag,msg_id))
	{
		frags_num = fragmenter_msg_frags(frag,msg_id);
		for(i=0;i<frags_num && complete;i++)
		{
			hdr = fragmenter_frag_header(&(frag->queue[msg_id%MAX_MSG_NUM][i]));
			if((hdr->frag_type) == FRAG_VOID)
				complete = 0;
		}

		return complete;
	}
	else
		return 0;
}

struct msghdr * fragmenter_frag_requests(const struct fragmenter *frag,uint32_t * req_num)
{
	struct msghdr * requests = NULL;
	uint16_t i;
	int16_t msgid;
	uint8_t j;
	uint32_t req_i=0;

	if(frag && req_num && *req_num)
	{
		requests = malloc((*req_num) * sizeof(struct msghdr));

		for (i=0;i<MAX_MSG_NUM && req_i<*req_num;i++)
		{
			msgid = fragmenter_queue_pos2msg_id(frag,i);
			if (msgid >= 0 && !(fragmenter_msg_complete(frag,msgid)))
				for(j=0;j<(fragmenter_msg_frags(frag,msgid)) && req_i<*req_num;j++)
					if(fragmenter_frag_type(fragmenter_get_frag(frag,msgid,j)) == FRAG_VOID)
						fragmenter_frag_init(&(requests[req_i++]),msgid,fragmenter_msg_frags(frag,msgid),j,NULL,0,FRAG_REQUEST);
		}
		if (req_i == 0)
		{
			free(requests);
			requests = NULL;
		}
	}
	if (req_num)
		*req_num = req_i;
	return requests;
}

int32_t fragmenter_add_frag(struct fragmenter *frag,const struct msghdr * fragment)
{
	struct msghdr * msg_frag;
	int32_t msg_id = -1;
	uint8_t frag_id,frags_num;
	struct my_hdr_t * hdr;

	if (fragment)
		hdr = fragmenter_frag_header(fragment);

	if (frag && fragment && hdr && hdr->frag_type == FRAG_DATA)
	{
		msg_id = hdr->message_id;
		frag_id = hdr->frag_seq;
		frags_num = hdr->frags_num;
		if (!fragmenter_msg_exists(frag,msg_id))
		{
			fragmenter_msg_remove(frag,msg_id);
			fragmenter_msg_init(frag,msg_id,frags_num);
		}
		msg_frag = &(frag->queue[msg_id%MAX_MSG_NUM][frag_id]); 
		memmove(msg_frag,fragment,sizeof(struct msghdr));
	}	

	return msg_id;
}


void fragmenter_dump_header(const struct my_hdr_t * hdr)
{
	fprintf(stderr,"MSG ID: %d\n",hdr->message_id);
	fprintf(stderr,"FRAG ID: %d\n",hdr->frag_seq);
	fprintf(stderr,"FRAGS NUM: %d\n",hdr->frags_num);
	fprintf(stderr,"FRAG TYPE: %d\n",hdr->frag_type);
}

void fragmenter_queue_dump(const struct fragmenter * frag)
{
	uint16_t i;
	fprintf(stderr,"fragmenter queue dump\n");
	for (i=0;i<MAX_MSG_NUM && frag!=NULL;i++)
	{
		if (fragmenter_msg_exists(frag,fragmenter_queue_pos2msg_id(frag,i)))
		{
			fprintf(stderr,"found msg: %d\n",i);
			if(fragmenter_msg_complete(frag,fragmenter_queue_pos2msg_id(frag,i)))
				fprintf(stderr,"\tmessage is complete\n");
			else
				fprintf(stderr,"\tmessage is incomplete\n");
		}
	}

}	

int fragmenter_pop_msg(struct fragmenter * frag,struct sockaddr_storage * msgname, uint8_t * buffer_ptr,const int buffer_size)
	// pop a complete msg from the queue
	// returns the number of bytes copied (which must be less than buffer_size)
	// if no message is available or in case of errors returns -1
{
	uint16_t i=0,selected_id;
	int in_bytes = -1, frag_datasize;
	struct msghdr * frag_msg;
	
	if(frag)
	{
		while(i<MAX_MSG_NUM && !(fragmenter_msg_complete(frag,fragmenter_queue_pos2msg_id(frag,i)))  )
			i++;

		if(i < MAX_MSG_NUM)
		{
			selected_id = fragmenter_queue_pos2msg_id(frag,i);
			in_bytes = 0;
			for (i=0;i<fragmenter_msg_frags(frag,selected_id) && buffer_ptr;i++)
			{
				frag_msg = fragmenter_get_frag(frag,selected_id,i);
				frag_datasize = frag_msg->msg_iov[1].iov_len;
				if ((in_bytes + frag_datasize) <= buffer_size)
				{
					memmove(&(buffer_ptr[in_bytes]),frag_msg->msg_iov[1].iov_base,frag_msg->msg_iov[1].iov_len);
					in_bytes += frag_msg->msg_iov[1].iov_len;
				}
			}
			if(msgname)
				fragmenter_frag_msgname(frag_msg,msgname);
			fragmenter_msg_remove(frag,selected_id);
			
		}
	}

	return in_bytes;
}

int16_t fragmenter_frag_type(const struct msghdr * frag_msg)
{
	struct my_hdr_t * hdr = NULL;
	if(frag_msg)
		hdr = fragmenter_frag_header(frag_msg);
	if (hdr && hdr->frag_seq < hdr->frags_num )
		return hdr->frag_type;
	return -1;
}
int32_t fragmenter_frag_msgid(const struct msghdr * frag_msg)
{
	struct my_hdr_t * hdr = NULL;
	if(frag_msg)
		hdr = fragmenter_frag_header(frag_msg);
	if (hdr)
		return hdr->message_id;
	return -1;
}
int16_t fragmenter_frag_id(const struct msghdr * frag_msg)
{
	struct my_hdr_t * hdr = NULL;
	if(frag_msg)
		hdr = fragmenter_frag_header(frag_msg);
	if (hdr && hdr->frag_seq <hdr->frags_num )
		return hdr->frag_seq;
	return -1;
}

uint8_t fragmenter_msg_frags(const struct fragmenter * frag,const uint16_t msg_id)
{	
	uint8_t frags_num = 0;
	struct my_hdr_t * hdr = NULL;
	if(frag && fragmenter_msg_exists(frag,msg_id))
	{
		hdr = fragmenter_frag_header(&(frag->queue[msg_id%MAX_MSG_NUM][0]));
	//	fragmenter_dump_header(hdr);
		frags_num = hdr->frags_num;
	}
//	printf("fraghi %d\n",hdr->frags_num);
	return frags_num;
}

