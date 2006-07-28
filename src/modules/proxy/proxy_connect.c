/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* CONNECT method for Apache proxy */

#include "mod_proxy.h"
#include "http_log.h"
#include "http_main.h"

#ifdef HAVE_BSTRING_H
#include <bstring.h>            /* for IRIX, FD_SET calls bzero() */
#endif

/*
 * This handles Netscape CONNECT method secure proxy requests.
 * A connection is opened to the specified host and data is
 * passed through between the WWW site and the browser.
 *
 * This code is based on the INTERNET-DRAFT document
 * "Tunneling SSL Through a WWW Proxy" currently at
 * http://www.mcom.com/newsref/std/tunneling_ssl.html.
 *
 * If proxyhost and proxyport are set, we send a CONNECT to
 * the specified proxy..
 *
 * FIXME: this is bad, because it does its own socket I/O
 *        instead of using the I/O in buff.c.  However,
 *        the I/O in buff.c blocks on reads, and because
 *        this function doesn't know how much data will
 *        be sent either way (or when) it can't use blocking
 *        I/O.  This may be very implementation-specific
 *        (to Linux).  Any suggestions?
 * FIXME: this doesn't log the number of bytes sent, but
 *        that may be okay, since the data is supposed to
 *        be transparent. In fact, this doesn't log at all
 *        yet. 8^)
 * FIXME: doesn't check any headers initally sent from the
 *        client.
 * FIXME: should allow authentication, but hopefully the
 *        generic proxy authentication is good enough.
 * FIXME: no check for r->assbackwards, whatever that is.
 */

static int allowed_port(proxy_server_conf *conf, int port)
{
    int i;
    int *list = (int *)conf->allowed_connect_ports->elts;

    for (i = 0; i < conf->allowed_connect_ports->nelts; i++) {
        if (port == list[i])
            return 1;
    }
    return 0;
}


int ap_proxy_connect_handler(request_rec *r, cache_req *c, char *url,
                                 const char *proxyhost, int proxyport)
{
    struct sockaddr_in server;
    struct addrinfo hints, *res, *res0;
    const char *hoststr;
    const char *portstr = NULL;
    char *p;
    int port, sock;
    char buffer[HUGE_STRING_LEN];
    int nbytes, i;
    fd_set fds;
    int error;

    void *sconf = r->server->module_config;
    proxy_server_conf *conf =
    (proxy_server_conf *)ap_get_module_config(sconf, &proxy_module);
    struct noproxy_entry *npent = (struct noproxy_entry *) conf->noproxies->elts;

    memset(&server, '\0', sizeof(server));
#ifdef HAVE_SOCKADDR_LEN
    server.sin_len = sizeof(server);
#endif
    server.sin_family = AF_INET;

    /* Break the URL into host:port pairs */

    hoststr = url;
    p = strchr(url, ':');
    if (p == NULL) {
	char pbuf[32];
	ap_snprintf(pbuf, sizeof(pbuf), "%d", DEFAULT_HTTPS_PORT);
	portstr = pbuf;
    } else {
	portstr = p + 1;
	*p = '\0';
    }
    port = atoi(portstr);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    error = getaddrinfo(hoststr, portstr, &hints, &res0);
    if (error && proxyhost == NULL) {
	return ap_proxyerror(r, HTTP_INTERNAL_SERVER_ERROR,
		    gai_strerror(error));       /* give up */
    }

/* check if ProxyBlock directive on this host */
    for (i = 0; i < conf->noproxies->nelts; i++) {
	int fail;
	struct sockaddr_in *sin;

	fail = 0;
	if (npent[i].name != NULL && strstr(hoststr, npent[i].name))
	    fail++;
	if (npent[i].name != NULL && strcmp(npent[i].name, "*") == 0)
	    fail++;
	for (res = res0; res; res = res->ai_next) {
	    switch (res->ai_family) {
	    case AF_INET:
		sin = (struct sockaddr_in *)res->ai_addr;
		if (sin->sin_addr.s_addr == npent[i].addr.s_addr)
		    fail++;
		break;
	    }
	}
	if (fail) {
	    if (res0 != NULL)
		freeaddrinfo(res0);
	    return ap_proxyerror(r, HTTP_FORBIDDEN,
				 "Connect to remote machine blocked");
	}
    }

    /* Check if it is an allowed port */
    if (conf->allowed_connect_ports->nelts == 0) {
        /* Default setting if not overridden by AllowCONNECT */
        switch (port) {
        case DEFAULT_HTTPS_PORT:
        case DEFAULT_SNEWS_PORT:
            break;
        default:
	    if (res0 != NULL)
		freeaddrinfo(res0);
            return HTTP_FORBIDDEN;
        }
    }
    else if (!allowed_port(conf, port)) {
	if (res0 == NULL)
	    freeaddrinfo(res0);
        return HTTP_FORBIDDEN;
    }

    if (proxyhost) {
	char pbuf[10];

	if (res0 != NULL)
	    freeaddrinfo(res0);
	ap_snprintf(pbuf, sizeof(pbuf), "%d", proxyport);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	error = getaddrinfo(proxyhost, pbuf, &hints, &res0);
	if (error)
	    return HTTP_INTERNAL_SERVER_ERROR;  /* XXX */

        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
             "CONNECT to remote proxy %s on port %d", proxyhost, proxyport);
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                     "CONNECT to %s on port %d", hoststr, port);
    }

    sock = -1;
    for (res = res0; res; res = res->ai_next) {
	sock = ap_psocket_ex(r->pool, res->ai_family, res->ai_socktype,
			     res->ai_protocol, 1);
	if (sock == -1)
	    continue;

#ifdef CHECK_FD_SETSIZE
    if (sock >= FD_SETSIZE) {
        ap_log_error(APLOG_MARK, APLOG_NOERRNO | APLOG_WARNING, NULL,
                     "proxy_connect_handler: filedescriptor (%u) "
                     "larger than FD_SETSIZE (%u) "
                     "found, you probably need to rebuild Apache with a "
                     "larger FD_SETSIZE", sock, FD_SETSIZE);
        ap_pclosesocket(r->pool, sock);
        return HTTP_INTERNAL_SERVER_ERROR;
    }
#endif

    i = ap_proxy_doconnect(sock, res->ai_addr, r);
    if (i == 0)
	break;
    }
    freeaddrinfo(res0);
    if (i == -1) {
        ap_pclosesocket(r->pool, sock);
        return ap_proxyerror(r, HTTP_INTERNAL_SERVER_ERROR, ap_pstrcat(r->pool,
        "Could not connect to remote machine:<br>", strerror(errno), NULL));
    }

    /*
     * If we are connecting through a remote proxy, we need to pass the
     * CONNECT request on to it.
     */
    if (proxyport) {
        /*
         * FIXME: We should not be calling write() directly, but we currently
         * have no alternative.  Error checking ignored.  Also, we force a
         * HTTP/1.0 request to keep things simple.
         */
        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                     "Sending the CONNECT request to the remote proxy");
        ap_snprintf(buffer, sizeof(buffer), "CONNECT %s HTTP/1.0" CRLF, r->uri);
        send(sock, buffer, strlen(buffer), 0);
        ap_snprintf(buffer, sizeof(buffer),
                    "Proxy-agent: %s" CRLF CRLF, ap_get_server_version());
        send(sock, buffer, strlen(buffer), 0);
    }
    else {
        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                     "Returning 200 OK Status");
        ap_rvputs(r, "HTTP/1.0 200 Connection established" CRLF, NULL);
        ap_rvputs(r, "Proxy-agent: ", ap_get_server_version(), CRLF CRLF, NULL);
        ap_bflush(r->connection->client);
    }

    while (1) {                 /* Infinite loop until error (one side closes
                                 * the connection) */
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        FD_SET(ap_bfileno(r->connection->client, B_WR), &fds);

        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                     "Going to sleep (select)");
        i = ap_select((ap_bfileno(r->connection->client, B_WR) > sock ?
                       ap_bfileno(r->connection->client, B_WR) + 1 :
                       sock + 1), &fds, NULL, NULL, NULL);
        ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                     "Woke from select(), i=%d", i);

        if (i) {
            if (FD_ISSET(sock, &fds)) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                             "sock was set");
                if ((nbytes = recv(sock, buffer, HUGE_STRING_LEN, 0)) != 0) {
                    if (nbytes == -1)
                        break;
                    if (send(ap_bfileno(r->connection->client, B_WR), buffer,
                             nbytes, 0) == EOF)
                        break;
                    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO,
                             r->server, "Wrote %d bytes to client", nbytes);
                }
                else
                    break;
            }
            else if (FD_ISSET(ap_bfileno(r->connection->client, B_WR), &fds)) {
                ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, r->server,
                             "client->fd was set");
                if ((nbytes = recv(ap_bfileno(r->connection->client, B_WR),
                                   buffer, HUGE_STRING_LEN, 0)) != 0) {
                    if (nbytes == -1)
                        break;
                    if (send(sock, buffer, nbytes, 0) == EOF)
                        break;
                    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO,
                             r->server, "Wrote %d bytes to server", nbytes);
                }
                else
                    break;
            }
            else
                break;          /* Must be done waiting */
        }
        else
            break;
    }

    ap_pclosesocket(r->pool, sock);

    return OK;
}
