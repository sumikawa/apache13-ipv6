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
 * This file includes getnameinfo().
 * These funtions are defined in rfc2133.
 *
 * But these functions are not implemented correctly. The minimum subset
 * is implemented for ssh use only. For exapmle, this routine assumes
 * that ai_family is AF_INET. Don't use it for another purpose.
 * 
 * In the case not using 'configure --enable-ipv6', this getnameinfo.c
 * will be used if you have broken getnameinfo or no getnameinfo.
 */

#if 0
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include "gai.h"
#endif

int
getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)
const struct sockaddr *sa;
size_t salen;
char *host;
size_t hostlen;
char *serv;
size_t servlen;
int flags;
{
  struct sockaddr_in *sin = (struct sockaddr_in *)sa;
  struct hostent *hp;
  char tmpserv[16];
  
  if (serv) {
    sprintf(tmpserv, "%d", ntohs(sin->sin_port));
    if (strlen(tmpserv) > servlen)
      return EAI_MEMORY;
    else
      strcpy(serv, tmpserv);
  }
  if (host)
    if (flags & NI_NUMERICHOST)
      if (strlen(inet_ntoa(sin->sin_addr)) > hostlen)
	return EAI_MEMORY;
      else {
	strcpy(host, inet_ntoa(sin->sin_addr));
	return 0;
      }
    else
      if (hp = gethostbyaddr((char *)&sin->sin_addr, sizeof(struct in_addr),
			     AF_INET))
	if (strlen(hp->h_name) > hostlen)
	  return EAI_MEMORY;
	else {
	  strcpy(host, hp->h_name);
	  return 0;
	}
      else
	return EAI_NODATA;
  return 0;
}
