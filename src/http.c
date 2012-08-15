/* spew http engine
 * Copyright (C) 2012 Varnish Software AS
 * Author: Kristian Lyngstøl, kristian@bohemians.org
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/sockios.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "param.h"
#include "inform.h"
#include "core.h"
#include "http.h"


static struct spew_eng_t spew_eng;

/*
 * Signal handler, mainly used for SIGINT.
 * Nice to have for gprof.
 */
void my_sighandler(int sig)
{
	if (sig == SIGINT) {
		inform(V(CORE), "Exiting on user request.");
		exit(0);
	}
}

/*
 * get_addr() takes care of name resolution.
 * Note that it leaves the result dangling un-freed. This is intentional as
 * it's used repeatedly. We need/want to keep the entire thing around so we
 * can switch between ipv4/ipv6 on the fly.
 */
static void get_addr(char *host, char *port)
{
	int s;
	struct addrinfo hints;
	assert(host);
	if (!port)
		port = "80";
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	s = getaddrinfo(host, port, &hints, &spew_eng.result);
	assert(s == 0);

}

/*
 * Open a single connection and set up various non-blocking socket options.
 * It's critical to get the nonblocking done BEFORE connect() so we don't
 * waste time waiting for the reply. epoll will let us know when we can
 * write to the fd.
 */
static int open_conn(int *sd)
{
	int sfd = -1, optval, ret;
	struct addrinfo *rp;
	for (rp = spew_eng.result; rp != NULL; rp = rp->ai_next) {
		TIME(sfd =
		     socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol));
		if (sfd == -1)
			continue;
		assert(fcntl(sfd, F_SETFL, O_NONBLOCK) == 0);
		optval = 1;
		assert(setsockopt
		       (sfd, SOL_TCP, TCP_NODELAY, &optval,
			sizeof(optval)) == 0);
		TIME(ret = connect(sfd, rp->ai_addr, rp->ai_addrlen));
		if (ret != -1 || errno == EINPROGRESS)
			break;
		TIME(close(sfd));
	}
	if (rp == NULL || sfd == -1) {
		inform(V(HTTP_CRIT),
		       "Failed to open a connection? Errno: %d:%s", errno,
		       strerror(errno));
		return 2;
	}
	optval = P_snd_buff();
	setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
	optval = P_rcv_buff();
	setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
	*sd = sfd;
	return 0;
}

/*
 * Kill a connection. Closes and removes the fd from epoll
 */
static void del_one(int fd)
{
	int ret;
	TIME(ret = epoll_ctl(spew_eng.epollfd, EPOLL_CTL_DEL, fd, NULL));
	assert(ret == 0);
	TIME(close(fd));
}

/*
 * Add a connection and inform epoll about it.
 *
 * FIXME: Need to handle BORK a bit better....
 * FIXME: And clean up in general...
 */
static int add_one(void)
{
	int ret, sfd = 0;
	struct epoll_event ev;
	while (1) {
		TIME(ret = open_conn(&sfd));
		if (ret == 0)
			break;
		inform(V(HTTP_CRIT), "BORK BORK on connect. Delaying 1s.\n");
		sleep(1);
	}

	ev.events =
	    EPOLLIN | EPOLLPRI | EPOLLET | EPOLLOUT | EPOLLHUP | EPOLLERR |
	    EPOLLRDHUP;
	ev.data.fd = sfd;
	TIME(ret = epoll_ctl(spew_eng.epollfd, EPOLL_CTL_ADD, sfd, &ev));
	if (ret == -1) {
		inform(V(HTTP_CRIT), "epoll_ctl failed. Errno: %d:%s",
		       errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
	spew_eng.fds[sfd] = 0;
	return sfd;
}

/*
 * Main loop.
 *
 * the ndel-usage can probably be removed. It's a left-over.
 */
static void epoll_loop(void)
{
	int n, sfd;
	int del_array[10000];
	int ndel = 0;
	int nfds;
	struct epoll_event events[MAX_EVENTS];

	for (;;) {
		nfds = epoll_wait(spew_eng.epollfd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			inform(V(HTTP_CRIT),
			       "epoll_wait error. Errno: %d:%s", errno,
			       strerror(errno));
			exit(EXIT_FAILURE);
		}
		ndel = 0;
		for (n = 0; n < nfds; ++n) {
			sfd = events[n].data.fd;
			if (events[n].events & (EPOLLPRI | EPOLLIN)) {
				TIME((*(spew_eng.data_engine->read))(spew_eng.data_engine, sfd));
			}
			if (events[n].events &
			    (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
				del_array[ndel++] = sfd;
			} else if (events[n].events & EPOLLOUT) {
				if (spew_eng.fds[sfd] == 0) {
					TIME((*spew_eng.data_engine->write)(spew_eng.data_engine, sfd));
					spew_eng.fds[sfd] = 1;
					shutdown(sfd, SHUT_WR);
				}
			}
		}
		for (ndel--; ndel >= 0; ndel--) {
			del_one(del_array[ndel]);
			add_one();
		}
	}
}

/*
 * Set up epoll and add all the connection for the first time.
 *
 * FIXME: This causes a bit of a humpty-dumpty ramp up since all
 *        connections get written to at the same time before any read-back
 *        starts...
 */
static void init_epoll(void)
{
	int i = 0;
	spew_eng.epollfd = epoll_create(MAX_EVENTS);
	if (spew_eng.epollfd == -1) {
		inform(V(HTTP_CRIT), "epoll_create failed. Errno: %d:%s",
		       errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < P_conns(); i++)
		add_one();
}

/*
 * Entry point.
 */
int http_main(void)
{
	if (!strcmp(P_data_writer(),"batch"))
		spew_eng.data_engine = &batch_data_engine;
	else
		assert("Bah");

	(*spew_eng.data_engine->init)(&batch_data_engine, P_url(), P_host_header());
	get_addr(P_server(), P_port());
	init_epoll();
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, my_sighandler);
	epoll_loop();
	return 0;
}
