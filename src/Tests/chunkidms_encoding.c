/*
 *  Copyright (c) 2009 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *
 *  This is free software; see gpl-3.0.txt
 */

 #include <stdint.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>
 #include <time.h>
 #include "chunkidms.h"
 #include "chunkidms_trade.h"


int fill_multiSet(struct chunkID_multiSet *cset)
{
    int i, res;
    for(i = 1; i < 20; i+=1){
      chunkID_multiSet_add_chunk(cset, 100+i,1);
      chunkID_multiSet_add_chunk(cset, 200+i,2);

    }

    res = chunkID_multiSet_total_size(cset);

    return res;
}


 static void simple_test(void)
 {
   struct chunkID_multiSet *cset;
   char config[32];

   sprintf(config,"size=%d",10);
   cset = chunkID_multiSet_init(config);
   if(!cset){
     fprintf(stderr,"Unable to allocate memory for rcset\n");

     return;
   }

   printf("Chunk ID Set initialised: size is %d\n", chunkID_multiSet_size(cset));
   fill_multiSet(cset);
   ChunkID_multiSet_print(stdout,cset);
   chunkID_multiSet_check(cset, 4,1);
   chunkID_multiSet_check(cset, 3,1);
   chunkID_multiSet_check(cset, 2,1);
   chunkID_multiSet_check(cset, 9,1);
   chunkID_multiSet_check(cset, 14,2);
   printf("Earliest chunk %d\nLatest chunk %d.\n",
           chunkID_multiSet_get_earliest(cset,1), chunkID_multiSet_get_latest(cset,1));
   chunkID_multiSet_clear(cset, 0, 1);
   free(cset);
 }



 static void encoding_test()
 {
   struct chunkID_multiSet *cset, *cset1;
   static uint8_t buff[2048];
   int res, meta_len;
   void *meta = strdup("I'm a beautiful string!");

   cset = chunkID_multiSet_init("size=0,single_size=1");
   if(!cset){
     fprintf(stderr,"Unable to allocate memory for rcset\n");

     return;
   }

   fill_multiSet(cset);
   res = encodeChunkMSSignaling(cset, meta, strlen(meta), buff, sizeof(buff));
   printf("Encoding Result: %d\n", res);
   chunkID_multiSet_clear_all(cset, 0);
   free(cset);
   meta=NULL;
   cset1 = decodeChunkMSSignaling(&meta, &meta_len, buff, res);
   ChunkID_multiSet_print(stdout,cset1);
   printf("META: %s\n",(char *)meta);
   chunkID_multiSet_clear_all(cset1, 0);
   free(cset1);
 }

 static void metadata_test(void)
 {
   struct chunkID_multiSet *cset;
   static uint8_t buff[2048];
   int res, meta_len;
   void *meta;

   meta = strdup("This is a test");
   res = encodeChunkMSSignaling(NULL, meta, strlen(meta) + 1, buff, sizeof(buff));
   printf("Encoding Result: %d\n", res);
   free(meta);

   for(int i=0; i<res; i++)
     printf("%hhx ",buff[i]);

   cset = decodeChunkMSSignaling(&meta, &meta_len, buff, res);
   printf("Decoded MetaData: %s (%d)\n", (char *)meta, meta_len);

   if (cset) {
     chunkID_multiSet_clear_all(cset, 0);
     free(cset);
   }

   free(meta);
 }
static void clear()
{
  struct chunkID_multiSet *cset;
  cset = chunkID_multiSet_init("size=0,single_size=1");
  fill_multiSet(cset);
  ChunkID_multiSet_print(stdout,cset);
  chunkID_multiSet_clear_all(cset, 0);
  printf("Cleared, total size: %d (should be 0).\n", chunkID_multiSet_total_size(cset));
  ChunkID_multiSet_print(stdout,cset);

}


static void singleset_encoding()
{
  //singleset_encoding_test();
}

 int main(int argc, char *argv[])
 {

   singleset_encoding();
   simple_test();
   encoding_test();
   metadata_test();
   clear();
   return 0;
 }
