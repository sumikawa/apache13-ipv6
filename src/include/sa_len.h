/* sa_len.h : tiny version of SA_LEN (written by <yoshfuji@ecei.tohoku.ac.jp>) */
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#ifndef HAVE_SOCKADDR_LEN
#ifndef SA_LEN
#define SA_LEN(s_)	ap_sa_len((s_)->sa_family)

static NET_SIZE_T ap_sa_len (sa_family_t af)
{
    switch (af){
#if defined(AF_INET)
    case AF_INET:
	return (sizeof(struct sockaddr_in));
#endif /* AF_INET */
#if defined(AF_INET6)
    case AF_INET6:
	return (sizeof(struct sockaddr_in6));
#endif
#ifdef AF_LOCAL
    case AF_LOCAL:
#endif /* AF_LOCAL */
#if defined(AF_UNIX) && (AF_UNIX != AF_LOCAL)
    case AF_UNIX:
#endif /* AF_UNIX */
#if defined(AF_FILE) && (AF_FILE != AF_LOCAL || AF_FILE != AF_UNIX)
    case AF_FILE:
#endif /* AF_FILE */
#if defined(AF_LOCAL) || defined(AF_UNIX) || defined(AF_FILE)
	return (sizeof(struct sockaddr_un));
#endif /* defined(AF_LOCAL) || defined(AF_UNIX) || defined(AF_FILE) */
    default:
	return 0;
    }
    return 0;
}
#endif /* SA_LEN */
#endif /* HAVE_SOCKADDR_LEN */
