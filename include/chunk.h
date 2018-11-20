#ifndef CHUNK_H
#define CHUNK_H

#include<stdint.h>

/**
 * @file chunk.h
 *
 * @brief Chunk structure.
 *
 * Describes the structure of the chunk.
 *
 */

typedef uint32_t flowid_t;
typedef int32_t chunkid_t;
typedef int32_t chunksize_t;


/**
 * Structure describing a chunk. This is part of the
 * public API
 */
typedef struct chunk {
   /**
    * Chunk ID. Should be unique in a stream, and is generally
    * an integer used as a sequence number.
    */
   chunkid_t id;
   /**
    * Size of the data, in byte.
    */
   chunksize_t size;
   /**
    * Pointer to a buffer containing the chunk payload.
    */
   uint8_t *data;
   /**
    * Chunk timestamp (can be the timestamp of the first frame in
    * the chunk, the chunk generation time, or something else).
    */
   uint64_t timestamp;
   /**
    * Pointer to an opaque structure containing some system-dependent
    * (or scheduler-dependent) data: for example, it can contain some
    * information from the video header, the ``chunk importance'' (in case
    * of media awareness and/or if layered encoding is used), the scheduling
    * deadline of the chunk, and so on...
    */
   void *attributes;
   /**
    * Size of the attributes, in byte.
    */
   chunksize_t attributes_size;
   /**
    * ID of the flow which the chunk belongs to (alias color)
    * Default is 1
   */
   flowid_t flow_id;
} Chunk;
#endif
