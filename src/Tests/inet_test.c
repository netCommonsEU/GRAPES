#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef _WIN32
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

int checkIPv4Translation()
{
	int res;
	const char *res1;
	struct sockaddr_storage s;
	struct sockaddr_storage c;
	char *buf = malloc(INET_ADDRSTRLEN);
	char *ipv4 = malloc(INET_ADDRSTRLEN);
	strcpy (ipv4, "10.23.23.10");
	printf("\tTranslating IPv4 string %s...",ipv4);
	res = inet_pton(AF_INET, ipv4, &((struct sockaddr_in *)&s)->sin_addr);
	if(!res)
	{
		perror ("inet");
		return res;
	}
	printf("to network %d\n", ((struct sockaddr_in *)&s)->sin_addr.s_addr);
	printf("\tTranslating IPv4 address %d...", ((struct sockaddr_in *)&s)->sin_addr.s_addr);
	res1 = inet_ntop(AF_INET, &((struct sockaddr_in *)&s)->sin_addr, buf, INET_ADDRSTRLEN);
	if(!res1)
	{
		perror ("inet");
		return res;
	}
	printf("to string %s\n", buf);
	res = inet_pton(AF_INET, buf, &((struct sockaddr_in *)&c)->sin_addr);
	return memcmp((const void *)&s,(const void *)&c,sizeof(c));
}

int checkIPv6Translation()
{
	int res, i;
	const char *res1;
	struct sockaddr_storage s;
	struct sockaddr_storage c;
	char *buf = malloc(INET6_ADDRSTRLEN);
	const char *ipv6 = "fe80:0000:0000:0000:0223:aeff:fe54:ea0c";
	printf("\tIPv6 Address string %s ", ipv6);
	res = inet_pton(AF_INET6, ipv6, &((struct sockaddr_in6 *)&s)->sin6_addr);
	if(!res)
	{
		perror ("inet");
		return res;
	}
	printf("to network ");
	for (i = 0; i < 16; i+=2)
        #if defined(_WIN32) || defined(__APPLE_CC__)
	   printf("%02x%02x:", (unsigned char) (((struct sockaddr_in6 *)&s)->sin6_addr.s6_addr[i]),(unsigned char) (((struct sockaddr_in6 *)&s)->sin6_addr.s6_addr[i+1]));
	#else
	    printf("%02x%02x:", (uint8_t) (((struct sockaddr_in6 *)&s)->sin6_addr.__in6_u.__u6_addr8[i]),(uint8_t) (((struct sockaddr_in6 *)&s)->sin6_addr.__in6_u.__u6_addr8[i+1]));
	#endif
	    printf("\n");
	printf("\tIPv6 Address ");
	for (i = 0; i < 16; i+=2)
               #if defined(_WIN32) || defined(__APPLE_CC__)
		   printf("%02x%02x:", (unsigned char) (((struct sockaddr_in6 *)&s)->sin6_addr.s6_addr[i]),(unsigned char) (((struct sockaddr_in6 *)&s)->sin6_addr.s6_addr[i+1]));
		#else
		    printf("%02x%02x:", (uint8_t) (((struct sockaddr_in6 *)&s)->sin6_addr.__in6_u.__u6_addr8[i]),(uint8_t) (((struct sockaddr_in6 *)&s)->sin6_addr.__in6_u.__u6_addr8[i+1]));
		#endif
	res1 = inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&s)->sin6_addr, buf, INET6_ADDRSTRLEN);
	if(!res1)
	{
		perror ("inet");
		return res;
	}
	printf(" to string %s\n",buf);
	res = inet_pton(AF_INET6, buf, &((struct sockaddr_in6 *)&c)->sin6_addr);
	return memcmp((const void *)&s,(const void *)&c,sizeof(c));
}

int main(int argc, char *argv[])
{
	int res = 0;
	if(!checkIPv4Translation())
	{
		fprintf(stderr, "IPv4 Translation failed");
		return 1;
	}
	printf("IPv4 Translation success\n");
	if(!checkIPv6Translation())
	{
		fprintf(stderr, "IPv6 Translation failed");
		return 1;
	}
	printf("IPv6 Translation success\n");
	return res;
}
