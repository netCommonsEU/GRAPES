/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2018 Massimo Girondi
 *
 *  This is free software; see lgpl-2.1.txt
 */

#ifndef CHUNKIDSET_H
#define CHUNKIDSET_H
#include<stdio.h>

/**
 * This struct reppresent a family of set (aka a set of set), it is intended to
 * be used in multiflow streaming environment.
 *
 * A basic cache function is given: for the majority of functions available,
 * if there is a serie of operations on the same flow_id, the cost of operation
 * is O(1) instead of O(n. of sets).
 *
 *
 */


/*
* Opaque data type representing a ChunkID Multiset
*/
typedef struct chunkID_multiSet ChunkIDMultiSet;
struct chunk_buffer;
struct chunk;


#define CHUNKID_INVALID (uint32_t)-1	/**< Trying to get a chunk ID from an empty set */



/**
 * @brief Allocate a chunk ID multiset.
 *
 * Create an empty chunk ID multiset, and return a pointer to it.
 *
 * @param size the number of flows to be contained (0 if not known)
 * @param single_size the number of chunks for each flow to be contained (0 if not known)
 *
 * @return the pointer to the new multiset on success, NULL on error
 */
struct chunkID_multiSet *chunkID_multiSet_init(int size, int single_size);


/**
 * @brief Clear a multiset and free all associated memory.
 *
 * @param h a pointer to the multiset
 */
void chunkID_multiSet_free(struct chunkID_multiSet *h);



/**
 * @brief Add a chunk ID to the multiset.
 *
 * Insert a chunk ID, and its associated priority (the priority is assumed
 * to depend on the insertion order), to the set. If the chunk
 * ID is already in the set, nothing happens.
 *
 * @param h a pointer to the set where the chunk ID has to be added
 * @param chunk_id the ID of the chunk to be inserted in the set
 * @param flow_id the flow_id of the chunk to be inserted in the set
 * @return > 0 if the chunk ID is correctly inserted in the set, 0 if chunk_id
 *         is already in the set, < 0 on error
 */
int chunkID_multiSet_add_chunk(struct chunkID_multiSet *h, int chunk_id, int flow_id);

 /**
  * @brief Get the number of chunkID inside a flow
  *
  * Return the number of chunkID in that flow
  *
  * @param h a pointer to the multiset
  * @param flow_id the id of the flow
  * @return the number of chunkID, -1 in case of error, 0 if empty
  */

int chunkID_multiSet_single_size(struct chunkID_multiSet *h, int flow_id);

 /**
  * @brief Get the number of set inside (i.e. the number of flows reppresented)
  *
  * Return the number of sets/flows.
  *
  * @param h a pointer to the multiset
  * @return the number of sets
  */
int chunkID_multiSet_size(const struct chunkID_multiSet *h);


/**
 * @brief Get the multiset size
 *
 * Return the total number of chunk IDs present in a multiset.
 * This is calculated as the sum of the size of every single_size
 * set/flow present in the multiset.
 *
 * @param h a pointer to the set
 * @return the number of chunk IDs in the set, or < 0 on error
 */
int chunkID_multiSet_total_size(const struct chunkID_multiSet *h);


/**
 * @brief Get a chunk ID from a multiset
 *
 * Return the i^th chunk ID from the set. The chunk's priority is
 * assumed to depend on i.
 *
 * @param h a pointer to the set
 * @param i the index of the chunk ID to be returned
 * @param flow_id the flow_id of the set containing the chunk ID
 *                 to be returned
 * @return the i^th chunk ID in the set with given flow_id in case of success, or < 0 on error
 *         (in case of error, priority is not meaningful)
 */
int chunkID_multiSet_get_chunk(struct chunkID_multiSet *h, int i, int flow_id);

/**
 * @brief Check if a chunk ID is in a multiset
 *
 * @param h a pointer to the multiset
 * @param chunk_id the chunk ID we are searching for
 * @param flow_id  the flow_id of the set where to search
 *
 * @return the priority of the chunk ID if it is present in the multiset,
 *         < 0 on error or if the chunk ID is not in the set
 */
int chunkID_multiSet_check(struct chunkID_multiSet *h, int chunk_id, int flow_id);


 /**
  * Clear a set
  *
  * Remove all the chunk IDs from the set with the given flow_id.
  *
  * @param h a pointer to the set
  * @param flow_id the flow_id of the set to be cleared
  * @param size the expected number of chunk IDs that will be stored
  *                 in the set; 0 if such a number is not known.
  */
void chunkID_multiSet_clear(struct chunkID_multiSet *h, int size, int flow_id);


/**
 * Clear a multiset
 *
 * Remove all the chunk IDs from all  the set.
 *
 * @param h a pointer to the set
 * @param size the expected number of chunk IDs that will be stored
 *                 in the set; 0 if such a number is not known.
 */

void chunkID_multiSet_clear_all(struct chunkID_multiSet *h, int size);



void chunkID_multiSet_trim(struct chunkID_multiSet *h, int size, int flow_id);

void chunkID_multiSet_trimAll(struct chunkID_multiSet *h, int size);


/**
 * @brief Get the smallest chunk ID from a set
 *
 * Return the ID of the earliest chunk from the the set for that flow_id.
 *
 * @param h a pointer to the multiset
 * @param flow_id the flow_id of the set
 * @return the chunk ID in case of success, or CHUNKID_INVALID on error
 */
uint32_t chunkID_multiSet_get_earliest(struct chunkID_multiSet *h, int flow_id);


/**
 * @brief Get the largest chunk ID from a set
 *
 * Return the ID of the latest chunk from the the set for that flow_id.
 *
 * @param h a pointer to the multiset
 * @param flow_id the flow_id of the set
 * @return the chunk ID in case of success, or CHUNKID_INVALID on error
 */
uint32_t chunkID_multiSet_get_latest(struct chunkID_multiSet *h, int flow_id);



/**
* Add chunks from a chunk ID multiset to another one
*
* Insert all chunk from a chunk ID set into another one. Priority is
* kept in the old one. New chunks from the added one are added with
* lower priorities, but keeping their order.
* The operation is repeated for each flow of the new set: if it contains
* new flows, they will be added to the multiset.
*
* @param h a pointer to the set where the chunk ID has to be added
* @param a a pointer to the set which has to be added
*/
void chunkID_multiSet_union(struct chunkID_multiSet *h, struct chunkID_multiSet *a);


 /**
  * @brief D Encode a sequence of information, filling the buffer with the corresponding bit stream.
  *
  * Encode a sequence of information given as parameters and fills a buffer (given as parameter) with the corresponding bit stream.
  * The main reason to encode and return the bit stream is the possibility to either send directly a packet with the encoded bit stream, or
  * add this bit stream in piggybacking
  *
  * @param[in] f pointer to a multiset of chunkID
  * @param[in] meta metadata associated to the ChunkID set
  * @param[in] meta_len length of the metadata
  * @param[in] buff Buffer that will be filled with the bit stream obtained as a coding of the above parameters
  * @param[in] buff_len length of the buffer that will contain the bit stream
  * @return 0 on success, <0 on error
  */
int encodeChunkMSSignaling(const struct chunkID_multiSet *f, const void *meta, int meta_len, uint8_t *buff, int buff_len);



/**
  * @brief Decode the bit stream.
  *
  * Decode the bit stream contained in the buffer, filling the other parameters. This is the dual of the encode function.
  *
  * @param[in] meta pointer to the metadata associated to the ChunkID multiset
  * @param[in] meta_len length of the metadata
  * @param[in] buff Buffer which contain the bit stream to decode, filling the above parameters
  * @param[in] buff_len length of the buffer that contain the bit stream
  * @return a pointer to the multiset of chunk ID set or NULL with meta-data when on success, NULL with empty values on error.
  */
struct chunkID_multiSet *decodeChunkMSSignaling(void **meta, int *meta_len, const uint8_t *buff, int buff_len);

/**
  * @brief Generate a multiSet from an array of ChunkBuffer
  *
  * Parse the chunkBuffers and generate the corresponding chunkID_multiSet.
  *
  * @param[in] cbs pointer to ChunkBuffers array
  * @param[in] len lenght of cbs
  * @return a pointer to the multiset of chunk ID set or NULL in case of error.
  */

struct chunkID_multiSet *generateChunkIDMultiSetFromChunkBuffers(struct chunk_buffer ** cbs, int len);

/**
  * @brief Forces the cached set to the one with given flow_id
  *
  * Set the cached set to the one with given flow_id.
  * This allow to pass only the pointer to the multiset to functions
  * and access to the cached flow_id to get the flow_id
  *
  * @param[in] ms the multiSet
  *
  * @param[in] flow_id the flow_id to set to cache
  */
void chunkID_multiSet_set_cached(struct chunkID_multiSet * ms, int flow_id);


/**
  * @brief Get the cached low_id
  *
  * Return the last flow_id passed to any multiset function
  * WARNING: USE THIS ONLY IF YOU ARE SURE NOTHING HAPPENS AFTER THE ChunkID_multiSet_set_cached on the
  * multiset object.
  * @param[in] ms the multiSet
  * @return the last flow_id
  */
int chunkID_multiSet_get_cached(struct chunkID_multiSet * ms);


/**
  * @brief Returns an array of known flows
  *
  * Return a pointer to an array of integers reppresenting the
  * sets contained in the multi_set.
  * @param[in] ms the multiSet
  * @param[out] size the size of the set (and the size of the array)
  *
  * @return the array with know flow_id
  *
  */
int * chunkID_multiSet_get_flows(struct chunkID_multiSet *ms, int * size);


/**
  * @brief Print the multiset to the given stream
  *
  * Print the content and some informations about on the passed file or stream pointer
  * @param[in] ms the multiSet
  * @param[in] stream the pointer to the stream to write
  *
  *
  */

void chunkID_multiSet_print(FILE * stream, const struct chunkID_multiSet * ms);

//Just a test function to see if everything works well
// void singleset_encoding_test();


#endif
