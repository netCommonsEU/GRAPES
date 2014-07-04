#include<assert.h>
#include<stdio.h>
#include<string.h>
#include<malloc.h>
#include<fragmenter.h>
#include<sys/socket.h>

#define HDR_SIZE 5

//uint16_t fragmenter_add_msg(struct fragmenter *frag,const uint8_t * buffer_ptr,const int buffer_size,const size_t frag_size);
///*split buffer in fragments and return the message_id*/
void fragmenter_add_msg_test()
{
	struct fragmenter * frag = fragmenter_init();
	struct sockaddr_storage addr;
	uint8_t buff[80];
	int32_t msgid;

	msgid = fragmenter_add_msg(NULL,NULL,-1,0);
	assert(msgid < 0);
	assert(fragmenter_msg_frags(frag,msgid) == 0);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) < 0);

	msgid = fragmenter_add_msg(frag,NULL,-1,0);
	assert(msgid < 0);
	assert(fragmenter_msg_frags(frag,msgid) == 0);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) < 0);

	msgid = fragmenter_add_msg(NULL,buff,-1,0);
	assert(msgid < 0);
	assert(fragmenter_msg_frags(frag,msgid) == 0);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) < 0);

	msgid = fragmenter_add_msg(frag,buff,-1,0);
	assert(msgid < 0);
	assert(fragmenter_msg_frags(frag,msgid) == 0);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) < 0);

	msgid = fragmenter_add_msg(frag,buff,0,0);
	assert(msgid < 0);
	assert(fragmenter_msg_frags(frag,msgid) == 0);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) < 0);

	msgid = fragmenter_add_msg(frag,buff,80,0);
	assert(msgid >= 0);
	assert(fragmenter_msg_frags(frag,msgid) == 80);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) == 80);

	msgid = fragmenter_add_msg(frag,buff,80,HDR_SIZE+10);
	assert(msgid >= 0);
	assert(fragmenter_msg_frags(frag,msgid) == 8);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) == 80);


	fragmenter_destroy(frag);
	printf("[PASSED] - %s\n",__func__);
}
//
//uint8_t fragmenter_msg_frags(const struct fragmenter *frag,const uint16_t msg_id);
///*get the number of fragments of a msg*/
void fragmenter_msg_frags_test()
{
	struct fragmenter * frag = fragmenter_init();

	assert(fragmenter_msg_frags(NULL,0) == 0);
	assert(fragmenter_msg_frags(frag,0) == 0);
	assert(fragmenter_msg_frags(frag,10) == 0);

	fragmenter_destroy(frag);
	printf("[PASSED] - %s\n",__func__);

}
//
//struct msghdr * fragmenter_get_frag(const struct fragmenter *frag,const uint16_t msg_id,const uint8_t frag_id);
///*return the specified fragment*/
void fragmenter_get_frag_test()
{
	struct fragmenter * frag = fragmenter_init();
	struct msghdr * frag_msg;
	uint16_t msgid;

	assert(fragmenter_get_frag(NULL,0,0)==NULL);
	assert(fragmenter_get_frag(frag,0,0)==NULL);

	msgid = fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	frag_msg = fragmenter_get_frag(frag,msgid,0);
	assert(frag_msg != NULL);

	frag_msg = fragmenter_get_frag(frag,msgid,3);
	assert(frag_msg != NULL);

	
	assert(fragmenter_get_frag(frag,msgid,4)==NULL);

	fragmenter_destroy(frag);
	printf("[PASSED] - %s\n",__func__);
}
//
//
///*incoming part*/
//int fragmenter_frag_shrink(struct msghdr * frag_msg,const int data_size);
//
void fragmenter_frag_shrink_test()
{
	struct msghdr frag_msg;
	
	assert(fragmenter_frag_shrink(NULL,0)<0);
	
	fragmenter_frag_init(&frag_msg,1,1,0,"ciao mondo",11,FRAG_DATA);
	fragmenter_frag_shrink(&frag_msg,HDR_SIZE+4);
	assert(strncmp(frag_msg.msg_iov[1].iov_base,"ciao",4) == 0);
	fragmenter_frag_deinit(&frag_msg);

	fragmenter_frag_init(&frag_msg,1,1,0,"ciao mondo",11,FRAG_DATA);
	assert(fragmenter_frag_shrink(&frag_msg,0) == 0);
	fragmenter_frag_deinit(&frag_msg);
	

	printf("[PASSED] - %s\n",__func__);
}

//uint16_t fragmenter_add_frag(struct fragmenter *frag,const struct msghdr * fragment);
void fragmenter_add_frag_test()
{
	struct msghdr frag_msg = {0};
	struct fragmenter * frag;

	assert(fragmenter_add_frag(NULL,NULL) < 0);
	frag = fragmenter_init();
	assert(fragmenter_add_frag(frag,NULL) < 0);
	assert(fragmenter_add_frag(frag,&frag_msg) < 0);

	fragmenter_frag_init(&frag_msg,1,1,0,"ciao mondo",11,FRAG_DATA);
	assert(fragmenter_add_frag(frag,&frag_msg)==1);
	fragmenter_frag_deinit(&frag_msg);

	fragmenter_frag_init(&frag_msg,1,1,0,"ciao mondo",11,FRAG_VOID);
	assert(fragmenter_add_frag(frag,&frag_msg)<0);
	fragmenter_frag_deinit(&frag_msg);

	fragmenter_frag_init(&frag_msg,1,1,0,"ciao mondo",11,FRAG_REQUEST);
	assert(fragmenter_add_frag(frag,&frag_msg)<0);
	fragmenter_frag_deinit(&frag_msg);

	fragmenter_destroy(frag);

	printf("[PASSED] - %s\n",__func__);
}
//int fragmenter_pop_msg(struct fragmenter * frag,struct sockaddr_storage * msg_name, uint8_t * buffer_ptr,const int buffer_size);
void fragmenter_pop_msg_test()
{
	struct fragmenter * frag;
	uint8_t buff[80];
	struct sockaddr_storage addr;
	
	assert(fragmenter_pop_msg(NULL,NULL,NULL,0) < 0);
	assert(fragmenter_pop_msg(NULL,&addr,buff,80) < 0);
	frag = fragmenter_init();

	assert(fragmenter_pop_msg(frag,NULL,buff,80) <  0);
	assert(fragmenter_pop_msg(frag,&addr,NULL,80) < 0);

	fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	assert(fragmenter_pop_msg(frag,&addr,buff,80) == 16);

	fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	assert(fragmenter_pop_msg(frag,NULL,buff,80) == 16);

	fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	assert(fragmenter_pop_msg(frag,&addr,buff,-1) == 0);

	fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	assert(fragmenter_pop_msg(frag,NULL,NULL,0) == 0);

	fragmenter_destroy(frag);
	printf("[PASSED] - %s\n",__func__);
}
//
//int32_t fragmenter_frag_msgid(const struct msghdr * frag_msg);
void fragmenter_frag_msgid_test()
{
	struct msghdr frag_msg;
	assert(fragmenter_frag_msgid(NULL) < 0);
	assert(fragmenter_frag_msgid(&frag_msg) < 0);

	fragmenter_frag_init(&frag_msg,1,1,0,"hello",6,FRAG_DATA);
	assert(fragmenter_frag_msgid(&frag_msg) == 1);

	printf("[PASSED] - %s\n",__func__);
}
//
//int16_t fragmenter_frag_id(const struct msghdr * frag_msg);
void fragmenter_frag_id_test()
{
	struct msghdr frag_msg = {0};
	assert(fragmenter_frag_id(NULL) < 0);
	assert(fragmenter_frag_id(&frag_msg) < 0);

	fragmenter_frag_init(&frag_msg,1,1,0,"hello",6,FRAG_DATA);
	assert(fragmenter_frag_id(&frag_msg) == 0);

	printf("[PASSED] - %s\n",__func__);
}
//
//int16_t fragmenter_frag_type(const struct msghdr * frag_msg);
void fragmenter_frag_type_test()
{
	struct msghdr frag_msg = {0};
	assert(fragmenter_frag_type(NULL) < 0);
	assert(fragmenter_frag_type(&frag_msg) < 0);

	fragmenter_frag_init(&frag_msg,1,1,0,"hello",6,FRAG_DATA);
	assert(fragmenter_frag_type(&frag_msg) == FRAG_DATA);

	fragmenter_frag_init(&frag_msg,1,1,0,"hello",6,FRAG_EXPIRED);
	assert(fragmenter_frag_type(&frag_msg) == FRAG_EXPIRED);

	printf("[PASSED] - %s\n",__func__);
}

//
//struct msghdr * fragmenter_frag_requests(const struct fragmenter *frag,uint32_t * req_num);
void fragmenter_frag_requests_test()
{
	struct fragmenter * frag;
	struct msghdr frag_msg,* requests;
	uint32_t req_num,msgid;

	assert(fragmenter_frag_requests(NULL,NULL) == NULL);
	frag = fragmenter_init();
	assert(fragmenter_frag_requests(frag,NULL) == NULL);

	req_num = 0;
	assert(fragmenter_frag_requests(frag,&req_num) == NULL);
	req_num = 1;
	assert(fragmenter_frag_requests(frag,&req_num) == NULL);

	msgid = fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	req_num = 0;
	assert(fragmenter_frag_requests(frag,&req_num) == NULL);
	req_num = 1;
	assert(fragmenter_frag_requests(frag,&req_num) == NULL);
	fragmenter_pop_msg(frag,NULL,NULL,0);
	
	msgid = fragmenter_frag_init(&frag_msg,1,3,1,"ciao mondo",11,FRAG_DATA);
	fragmenter_add_frag(frag,&frag_msg);
	req_num = 0;
	requests = fragmenter_frag_requests(frag,&req_num);
	assert( requests == NULL);
	assert(req_num == 0);
	req_num = 1;
	requests = fragmenter_frag_requests(frag,&req_num);
	assert( requests != NULL);
	free(requests);
	assert(req_num == 1);
	req_num = 2;
	requests = fragmenter_frag_requests(frag,&req_num);
	assert( requests != NULL);
	free(requests);
	assert(req_num == 2);
	req_num = 3;
	requests = fragmenter_frag_requests(frag,&req_num);
	assert( requests != NULL);
	assert(req_num == 2);

	assert(fragmenter_frag_msgid(requests) == 1);
	printf("frags num: %d\n",fragmenter_msg_frags(frag,msgid));
	assert(fragmenter_msg_frags(frag,msgid) == 3);
	assert(fragmenter_frag_id(requests) == 0);

	free(requests);

	fragmenter_frag_deinit(&frag_msg);
	fragmenter_destroy(frag);
	printf("[PASSED] - %s\n",__func__);
}
//
//void fragmenter_msg_remove(struct fragmenter *frag,const uint16_t msg_id);
void fragmenter_msg_remove_test()
{
	struct fragmenter * frag;
	uint16_t msgid;

	fragmenter_msg_remove(NULL,0);
	frag = fragmenter_init();
	fragmenter_msg_remove(frag,0);
	
	msgid = fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	fragmenter_msg_remove(frag,msgid);
	assert(fragmenter_pop_msg(frag,NULL,NULL,0) < 0);

	fragmenter_destroy(frag);
	printf("[PASSED] - %s\n",__func__);
}
//
//uint8_t fragmenter_msg_exists(const struct fragmenter *frag,const uint16_t msg_id);
void fragmenter_msg_exists_test()
{
	struct fragmenter *frag;
	uint16_t msgid;

	assert(!fragmenter_msg_exists(NULL,0));
	frag = fragmenter_init();
	assert(!fragmenter_msg_exists(NULL,45));
	msgid = fragmenter_add_msg(frag,"1234567890abcdef",16,HDR_SIZE+4);
	assert(fragmenter_msg_exists(NULL,msgid));

	printf("[PASSED] - %s\n",__func__);
}

//uint8_t fragmenter_frag_init(struct msghdr * frag_msg,const uint16_t msg_id,const uint8_t frags_num ,const uint8_t frag_id,const uint8_t* data_ptr,const int data_size,const uint8_t type);
void fragmenter_frag_init_test()
{
	struct msghdr frag_msg;
	
	assert(fragmenter_frag_init(NULL,0,1,0,NULL,0,0) >  0);

	assert(fragmenter_frag_init(&frag_msg,0,1,0,NULL,0,0) == 0);
	fragmenter_frag_deinit(&frag_msg);
	
	assert(fragmenter_frag_init(&frag_msg,0,1,1,NULL,0,0) > 0);

	assert(fragmenter_frag_init(&frag_msg,0,1,0,"ciao",5,0) == 0);
	fragmenter_frag_deinit(&frag_msg);
	
	assert(fragmenter_frag_init(&frag_msg,5,4,3,"ciao",5,7) == 0);
	assert(fragmenter_frag_msgid(&frag_msg) == 5);
	assert(fragmenter_frag_id(&frag_msg) == 3);
	assert(fragmenter_frag_type(&frag_msg) == 7);
	assert(strncmp(frag_msg.msg_iov[1].iov_base,"ciao",4) == 0);
	assert(frag_msg.msg_iov[1].iov_len == 5);
	fragmenter_frag_deinit(&frag_msg);

	printf("[PASSED] - %s\n",__func__);
}
//
//void fragmenter_frag_deinit(struct msghdr * frag_msg);
void fragmenter_frag_deinit_test()
{
	struct msghdr frag_msg;

	fragmenter_frag_deinit(NULL);
	fragmenter_frag_init(&frag_msg,5,4,3,"ciao",5,7);
	fragmenter_frag_deinit(&frag_msg);
	assert(frag_msg.msg_iov == NULL);
	
	printf("[PASSED] - %s\n",__func__);
}
//
int main()
{
	fragmenter_add_msg_test();
	fragmenter_frag_shrink_test();
	fragmenter_get_frag_test();
	fragmenter_msg_frags_test();
	fragmenter_add_frag_test();
	fragmenter_frag_init_test();
	fragmenter_frag_deinit_test();
	fragmenter_pop_msg_test();
	fragmenter_frag_msgid_test();
	fragmenter_frag_id_test();
	fragmenter_frag_type_test();
	fragmenter_frag_requests_test();
	fragmenter_msg_remove_test();
	fragmenter_msg_exists_test();
	return 0;
}
