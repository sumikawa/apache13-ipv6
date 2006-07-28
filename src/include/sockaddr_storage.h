/*
struct sockaddr_storage

   RFC2553 proposes struct sockaddr_storage.
   This is a placeholder for all sockaddr-variant structures. This is
   implemented like follows:

   You should use this structure to hold any of sockaddr-variant
   structures.
*/
#ifdef NEED_SOCKADDR_STORAGE

struct sockaddr_storage {
#ifdef HAVE_SOCKADDR_LEN
	u_char ss_len;
	u_char ss_family;
#else
	u_short ss_family;
#endif
	u_char __padding[128 - 2];
};

/*
union sockunion

   Alternatively, you may want to implement sockunion.h, with the
   following content:

   NOTE: For better portability, struct sockaddr_storage should be used.
   union sockunion is okay, but is not really portable enough.
*/
union sockunion {
	struct sockinet {
#ifdef HAVE_SOCKADDR_LEN
		u_char si_len;
		u_char si_family;
#else
		u_short si_family;
#endif
		u_short si_port;
	} su_si;
	struct sockaddr_in  su_sin;
#ifdef INET6
	struct sockaddr_in6 su_sin6;
#endif
};
#ifdef HAVE_SOCKADDR_LEN
#define su_len		su_si.si_len
#endif
#define su_family	su_si.si_family
#define su_port		su_si.si_port

#endif /* NEED_SOCKADDR_STORAGE */
