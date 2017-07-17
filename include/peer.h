/**
 * @file:   peer.h
 * @brief Peer structure definition.
 * @author: Alessandro Russo <russo@disi.unitn.it>
 *
 * @date December 15, 2009, 2:09 PM
 */

#ifndef _PEER_H
#define	_PEER_H

#include <sys/time.h>

struct peer {
    /* Peer identifier, the nodeid associated with the peer */
    struct nodeID *id;

    /* Peer creation time */
    struct timeval creation_timestamp; 

    /* User defined data to be broadcasted in the network */
    void * metadata; 

    /* User defined data not to be sent over the network */
    void * user_data;
};


#endif	/* _PEER_H */
