/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2010 Csaba Kiraly
 *  Copyright (c) 2010 Alessandro Russo
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#else
#define _WIN32_WINNT 0x0501 /* WINNT>=0x501 (WindowsXP) for supporting getaddrinfo/freeaddrinfo.*/
#include "win32-net.h"
#endif

#include "net_helper.h"

#define MAX_MSG_SIZE 1024 * 60
enum L3PROTOCOL {IPv4, IPv6} l3 = IPv4;

struct nodeID {
  struct sockaddr_storage addr;
  int fd;
};

int wait4data(const struct nodeID *s, struct timeval *tout, int *user_fds)
{
  fd_set fds;
  int i, res, max_fd;

  FD_ZERO(&fds);
  if (s) {
    max_fd = s->fd;
    FD_SET(s->fd, &fds);
  } else {
    max_fd = -1;
  }
  if (user_fds) {
    for (i = 0; user_fds[i] != -1; i++) {
      FD_SET(user_fds[i], &fds);
      if (user_fds[i] > max_fd) {
        max_fd = user_fds[i];
      }
    }
  }
  res = select(max_fd + 1, &fds, NULL, NULL, tout);
  if (res <= 0) {
    return res;
  }
  if (s && FD_ISSET(s->fd, &fds)) {
    return 1;
  }

  /* If execution arrives here, user_fds cannot be 0
     (an FD is ready, and it's not s->fd) */
  for (i = 0; user_fds[i] != -1; i++) {
    if (!FD_ISSET(user_fds[i], &fds)) {
      user_fds[i] = -2;
    }
  }

  return 2;
}

struct nodeID *create_node(const char *IPaddr, int port)
{
  struct nodeID *s;
  int res;
  struct addrinfo hints, *result;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_NUMERICHOST;

  s = malloc(sizeof(struct nodeID));
  memset(s, 0, sizeof(struct nodeID));

  if ((res = getaddrinfo(IPaddr, NULL, &hints, &result)))
  {
    fprintf(stderr, "Cannot resolve hostname '%s'\n", IPaddr);
    return NULL;
  }
  s->addr.ss_family = result->ai_family;
  switch (result->ai_family)
  {
    case (AF_INET):
      ((struct sockaddr_in *)&s->addr)->sin_port = htons(port);
      res = inet_pton (result->ai_family, IPaddr, &((struct sockaddr_in *)&s->addr)->sin_addr);
    break;
    case (AF_INET6):
      ((struct sockaddr_in6 *)&s->addr)->sin6_port = htons(port);
      res = inet_pton (result->ai_family, IPaddr, &(((struct sockaddr_in6 *) &s->addr)->sin6_addr));
    break;
    default:
      fprintf(stderr, "Cannot resolve address family %d for '%s'\n", result->ai_family, IPaddr);
      res = 0;
      break;
  }
  freeaddrinfo(result);
  if (res != 1)
  {
    fprintf(stderr, "Could not convert address '%s'\n", IPaddr);
    free(s);

    return NULL;
  }

  s->fd = -1;

  return s;
}

struct nodeID *net_helper_init(const char *my_addr, int port, const char *config)
{
  int res;
  struct nodeID *myself;

  myself = create_node(my_addr, port);
  if (myself == NULL) {
    fprintf(stderr, "Error creating my socket (%s:%d)!\n", my_addr, port);

    return NULL;
  }
  myself->fd =  socket(myself->addr.ss_family, SOCK_DGRAM, 0);
  if (myself->fd < 0) {
    free(myself);

    return NULL;
  }
//  TODO:
//  if (addr->sa_family == AF_INET6) {
//      r = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
//  }

  fprintf(stderr, "My sock: %d\n", myself->fd);

  switch (myself->addr.ss_family)
  {
    case (AF_INET):
        res = bind(myself->fd, (struct sockaddr *)&myself->addr, sizeof(struct sockaddr_in));
    break;
    case (AF_INET6):
        res = bind(myself->fd, (struct sockaddr *)&myself->addr, sizeof(struct sockaddr_in6));
    break;
    default:
      fprintf(stderr, "Cannot resolve address family %d in bind\n", myself->addr.ss_family);
      res = 0;
    break;
  }

  if (res < 0) {
    /* bind failed: not a local address... Just close the socket! */
    close(myself->fd);
    free(myself);

    return NULL;
  }

  return myself;
}

void bind_msg_type (uint8_t msgtype)
{
}

struct my_hdr_t {
  uint8_t m_seq;
  uint8_t frag_seq;
  uint8_t frags;
} __attribute__((packed));

int send_to_peer(const struct nodeID *from, struct nodeID *to, const uint8_t *buffer_ptr, int buffer_size)
{
  struct msghdr msg = {0};
  static struct my_hdr_t my_hdr;
  struct iovec iov[2];
  int res;

  iov[0].iov_base = &my_hdr;
  iov[0].iov_len = sizeof(struct my_hdr_t);
  msg.msg_name = &to->addr;
  msg.msg_namelen = sizeof(struct sockaddr_storage);
  msg.msg_iovlen = 2;
  msg.msg_iov = iov;

  my_hdr.m_seq++;
  my_hdr.frags = (buffer_size / (MAX_MSG_SIZE)) + 1;
  my_hdr.frag_seq = 0;

  do {
    iov[1].iov_base = buffer_ptr;
    if (buffer_size > MAX_MSG_SIZE) {
      iov[1].iov_len = MAX_MSG_SIZE;
    } else {
      iov[1].iov_len = buffer_size;
    }
    my_hdr.frag_seq++;

    buffer_size -= iov[1].iov_len;
    buffer_ptr += iov[1].iov_len;
    res = sendmsg(from->fd, &msg, 0);

    if (res  < 0){
      int error = errno;
      fprintf(stderr,"net-helper: sendmsg failed errno %d: %s\n", error, strerror(error));
    }
  } while (buffer_size > 0);

  return res;
}

int recv_from_peer(const struct nodeID *local, struct nodeID **remote, uint8_t *buffer_ptr, int buffer_size)
{
  int res, recv, m_seq, frag_seq;
  struct sockaddr_storage raddr;
  struct msghdr msg = {0};
  static struct my_hdr_t my_hdr;
  struct iovec iov[2];

  iov[0].iov_base = &my_hdr;
  iov[0].iov_len = sizeof(struct my_hdr_t);
  msg.msg_name = &raddr;
  msg.msg_namelen = sizeof(struct sockaddr_storage);
  msg.msg_iovlen = 2;
  msg.msg_iov = iov;

  *remote = malloc(sizeof(struct nodeID));
  if (*remote == NULL) {
    return -1;
  }

  recv = 0;
  m_seq = -1;
  frag_seq = 0;
  do {
    iov[1].iov_base = buffer_ptr;
    if (buffer_size > MAX_MSG_SIZE) {
      iov[1].iov_len = MAX_MSG_SIZE;
    } else {
      iov[1].iov_len = buffer_size;
    }
    buffer_size -= iov[1].iov_len;
    buffer_ptr += iov[1].iov_len;
    res = recvmsg(local->fd, &msg, 0);
    recv += (res - sizeof(struct my_hdr_t));
    if (m_seq != -1 && my_hdr.m_seq != m_seq) {
      return -1;
    } else {
      m_seq = my_hdr.m_seq;
    }
    if (my_hdr.frag_seq != frag_seq + 1) {
      return -1;
    } else {
     frag_seq++;
    }
  } while ((my_hdr.frag_seq < my_hdr.frags) && (buffer_size > 0));
  memcpy(&(*remote)->addr, &raddr, msg.msg_namelen);
  (*remote)->fd = -1;

  return recv;
}

int node_addr(const struct nodeID *s, char *addr, int len)
{
  int n;

  if (node_ip(s, addr, len) < 0) {
    return -1;
  }
  n = snprintf(addr + strlen(addr), len - strlen(addr) - 1, ":%d", node_port(s));

  return n;
}

struct nodeID *nodeid_dup(struct nodeID *s)
{
  struct nodeID *res;

  res = malloc(sizeof(struct nodeID));
  if (res != NULL) {
    memcpy(res, s, sizeof(struct nodeID));
  }

  return res;
}

int nodeid_equal(const struct nodeID *s1, const struct nodeID *s2)
{
  return (memcmp(&s1->addr, &s2->addr, sizeof(struct sockaddr_storage)) == 0);
}

int nodeid_cmp(const struct nodeID *s1, const struct nodeID *s2)
{
  return memcmp(&s1->addr, &s2->addr, sizeof(struct sockaddr_storage));
}

int nodeid_dump(uint8_t *b, const struct nodeID *s, size_t max_write_size)
{
  if (max_write_size < sizeof(struct sockaddr_storage)) return -1;

  memcpy(b, &s->addr, sizeof(struct sockaddr_storage));

  return sizeof(struct sockaddr_storage);
}

struct nodeID *nodeid_undump(const uint8_t *b, int *len)
{
  struct nodeID *res;
  res = malloc(sizeof(struct nodeID));
  if (res != NULL) {
    memcpy(&res->addr, b, sizeof(struct sockaddr_storage));
    res->fd = -1;
  }
  *len = sizeof(struct sockaddr_storage);

  return res;
}

void nodeid_free(struct nodeID *s)
{
  free(s);
}

int node_ip(const struct nodeID *s, char *ip, int len)
{
  switch (s->addr.ss_family)
  {
    case AF_INET:
      inet_ntop(s->addr.ss_family, &((const struct sockaddr_in *)&s->addr)->sin_addr, ip, len);
      break;
    case AF_INET6:
      inet_ntop(s->addr.ss_family, &((const struct sockaddr_in6 *)&s->addr)->sin6_addr, ip, len);
      break;
    default:
      break;
  }
  if (!ip) {
	  perror("inet_ntop");
	  return -1;
  }
  return 1;
}

int node_port(const struct nodeID *s)
{
  int res;
  switch (s->addr.ss_family)
  {
    case AF_INET:
      res = ntohs(((const struct sockaddr_in *) &s->addr)->sin_port);
      break;
    case AF_INET6:
      res = ntohs(((const struct sockaddr_in6 *)&s->addr)->sin6_port);
      break;
    default:
      res = -1;
      break;
  }
  return res;
}
