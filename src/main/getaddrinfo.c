/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * fake library for ssh v6 enabler patch
 *
 * This file includes getaddrinfo(), freeaddrinfo() and gai_strerror().
 * These funtions are defined in rfc2133.
 *
 * But these functions are not implemented correctly. The minimum subset
 * is implemented for ssh use only. For exapmle, this routine assumes
 * that ai_family is AF_INET. Don't use it for another purpose.
 * 
 * In the case not using 'configure --enable-ipv6', this getaddrinfo.c
 * will be used if you have broken getaddrinfo or no getaddrinfo.
 */

#if 0
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "gai.h"
#endif

static struct addrinfo *
malloc_ai(port, addr, socktype, protocol)
int port;
u_long addr;
int socktype;
int protocol;
{
  struct addrinfo *ai;

  if (ai = (struct addrinfo *)malloc(sizeof(struct addrinfo) +
				     sizeof(struct sockaddr_in))) {
    memset(ai, 0, sizeof(struct addrinfo) + sizeof(struct sockaddr_in));
    ai->ai_addr = (struct sockaddr *)(ai + 1);
#if defined(HAVE_SOCKADDR_LEN)
    ai->ai_addr->sa_len = 
#endif
      ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_addr->sa_family = ai->ai_family = AF_INET;
    ai->ai_socktype = socktype;
    ai->ai_protocol = protocol;
    ((struct sockaddr_in *)(ai)->ai_addr)->sin_port = port;
    ((struct sockaddr_in *)(ai)->ai_addr)->sin_addr.s_addr = addr;
    return ai;
  } else {
    return NULL;
  }
}

char *
gai_strerror(ecode)
int ecode;
{
  switch (ecode) {
  case EAI_NODATA:
    return "no address associated with hostname.";
  case EAI_MEMORY:
    return "memory allocation failure.";
  default:
    return "unknown error.";
  }
}    

void
freeaddrinfo(ai)
struct addrinfo *ai;
{
  struct addrinfo *next;
  
  do {
    next = ai->ai_next;
    free(ai);
  } while (ai = next);
}

int
getaddrinfo(hostname, servname, hints, res)
const char *hostname, *servname;
const struct addrinfo *hints;
struct addrinfo **res;
{
  struct addrinfo *cur, *prev = NULL;
  struct hostent *hp;
  int i, port;
  
  if (servname)
    port = htons(atoi(servname));
  else
    port = 0;
  if (hints && hints->ai_flags & AI_PASSIVE)
    if (*res = malloc_ai(port, htonl(0x00000000), 
			 res->ai_socktype ? res->ai_socktype : SOCK_STREAM,
			 res->ai_protocol))
      return 0;
    else
      return EAI_MEMORY;
  if (!hostname)
    if (*res = malloc_ai(port, htonl(0x7f000001),
			 res->ai_socktype ? res->ai_socktype : SOCK_STREAM,
			 res->ai_protocol))
      return 0;
    else
      return EAI_MEMORY;
  if (inet_addr(hostname) != -1)
    if (*res = malloc_ai(port, inet_addr(hostname),
			 res->ai_socktype ? res->ai_socktype : SOCK_STREAM,
			 res->ai_protocol))
      return 0;
    else
      return EAI_MEMORY;
  if ((hp = gethostbyname(hostname)) &&
      hp->h_name && hp->h_name[0] && hp->h_addr_list[0]) {
    for (i = 0; hp->h_addr_list[i]; i++)
      if (cur = malloc_ai(port,
			  ((struct in_addr *)hp->h_addr_list[i])->s_addr,
			  res->ai_socktype ? res->ai_socktype : SOCK_STREAM,
			  res->ai_protocol)) {
	if (prev)
	  prev->ai_next = cur;
	else
	  *res = cur;
	prev = cur;
      } else {
	if (*res)
	  freeaddrinfo(*res);
	return EAI_MEMORY;
      }
    return 0;
  }
  return EAI_NODATA;
}
