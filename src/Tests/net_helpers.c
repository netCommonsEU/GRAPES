/*
 *  Copyright (c) 2010 Luca Abeni
 *  Copyright (c) 2010 Csaba Kiraly
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <sys/types.h>
#ifndef _WIN32
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>     /* For struct ifreq */
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "net_helpers.h"

extern enum L3PROTOCOL {IPv4, IPv6} l3;


const char *iface_addr(const char *iface)
{
#ifndef _WIN32
    struct ifaddrs *if_addr, *ifap;
	int family;
	char *host_addr;
	int ifcount;

	if (getifaddrs(&if_addr) == -1)
	{
	  perror("getif_addrs");
	  return NULL;
	}
	ifcount = 0;
	for (ifap = if_addr; ifap != NULL; ifap = ifap->ifa_next)
	{
		if (ifap->ifa_addr == NULL)
		{
			ifcount++;
			continue;
		}
		family = ifap->ifa_addr->sa_family;
		if (l3 == IPv4 && family == AF_INET && !strcmp (ifap->ifa_name, iface))
		{
			host_addr = malloc((size_t)INET_ADDRSTRLEN);
			if (!host_addr)
			{
				perror("malloc host_addr");
				return NULL;
			}
			if (!inet_ntop(AF_INET, (void *)&(((struct sockaddr_in *)(ifap->ifa_addr))->sin_addr), host_addr, INET_ADDRSTRLEN))
			{
				perror("inet_ntop");
				return NULL;
			}
			break;
		}
		if (l3 == IPv6 && family == AF_INET6 && !strcmp (ifap->ifa_name, iface))
		{
			host_addr = malloc((size_t)INET6_ADDRSTRLEN);
			if (!host_addr)
			{
				perror("malloc host_addr");
				return NULL;
			}
			if (!inet_ntop(AF_INET6, (void *)&(((struct sockaddr_in6 *)(ifap->ifa_addr))->sin6_addr), host_addr, INET6_ADDRSTRLEN))
			{
				perror("inet_ntop");
				return NULL;
			}
			break;
		}

	}
	freeifaddrs(if_addr);
	return host_addr;
#else
    if(iface != NULL && strcmp(iface, "lo") == 0) return (l3==IPv4?"127.0.0.1":"::1");
    if(iface != NULL && inet_addr(iface) != INADDR_NONE) return strdup(iface);
    return (l3==IPv4?"127.0.0.1":"::1");
#endif
}
