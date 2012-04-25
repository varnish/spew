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

/*
 * Fugly
 */
#define MAX_EVENTS 1000
#define MAX_FDS 10000000

struct data_set_t {
	char *data;
	char *batch;
	size_t dsize;
	size_t bsize;
} data_set;

struct spew_eng_t {
	int fds[MAX_FDS];
	int epollfd;
	struct addrinfo *result;
} spew_eng;

/*
 * Estimate how slow the operation s is and print it if it exceeds
 * P_timer_threshold().
 *
 * FIXME: Probably doesn't belong here.
 */
#define TIMEa(s) do { s; } while(0)
#define TIME(s) do { 								\
		struct timeval timerstat_1;					\
		struct timeval timerstat_2; 					\
		suseconds_t diff; 						\
		gettimeofday(&timerstat_1, NULL); 				\
		s; 								\
		gettimeofday(&timerstat_2, NULL); 				\
		diff = timerstat_2.tv_usec - timerstat_1.tv_usec; 		\
		if (diff > P_timer_threshold()) 				\
			inform(V(HTTP_DEBUG), "%s took %lu µs", #s, diff);	\
		} while (0)

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
 * Pre-creates the entire HTTP request. Petty dirty.
 *
 * It sticks Connection: close on the last request to trick the server into
 * doing out job for us. It's simpler.
 *
 * FIXME: UUUGH.
 */
static void build_data_set(char *url, char *host)
{
	char *buf = malloc(1024);
	char tmp[1024];
	char *batch;
	int i;
	assert(buf);
	assert(url);
	assert(host);
	assert(strlen(url) < 256);
	assert(strlen(host) < 256);
	snprintf(buf, 1024, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", url, host);
	snprintf(tmp, 1024,
		 "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
		 url, host);
	data_set.data = buf;
	data_set.dsize = strlen(buf);
	batch =
	    malloc(((P_reqs() - 1) * data_set.dsize) + P_reqs() + strlen(tmp));
	assert(batch);
	data_set.batch = batch;
	for (i = 0; i < (P_reqs() - 1); i++) {
		memcpy(batch, data_set.data, data_set.dsize);
		batch = batch + data_set.dsize;
	}
	memcpy(batch, tmp, strlen(tmp));
	batch = batch + strlen(tmp);
	data_set.bsize = batch - data_set.batch;
	sleep(1);
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
 * Writes the data needed. Not all that good...
 *
 * FIXME: Even if we do get that write failure, we have no way of
 *        recovering from it since we don't track the progress.... Hmm.
 *        Could probably use spew_eng.fds[] for that.
 */
static int write_data(int fd)
{
	int ret;
	ret = write(fd, data_set.batch, data_set.bsize);
	if (ret <= 0 && errno != EAGAIN) {
		inform(V(HTTP_CRIT), "Write failed. Errno: %d:%s", errno,
		       strerror(errno));
		return -1;
	}
	return 0;
}

/*
 * Soaks up all data on the fd. Mainly intended to empty buffers.
 * Probably want to do a bit of parsing here.
 *
 * FIXME: I'm sure there are better ways....
 */
static void read_up(int fd)
{
	int ret;
	char buf[16384];
	for (ret = 1; ret > 0;)
		ret = read(fd, buf, 16383);

	if (ret < 0 && errno != EAGAIN)
		inform(V(HTTP_INFO), "Read error. Ret: %d.  Errno: %d:%s",
		       ret, errno, strerror(errno));
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
				TIME(read_up(sfd));
			}
			if (events[n].events &
			    (EPOLLRDHUP | EPOLLERR | EPOLLHUP)) {
				del_array[ndel++] = sfd;
			} else if (events[n].events & EPOLLOUT) {
				if (spew_eng.fds[sfd] == 0) {
					TIME(write_data(sfd));
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
	build_data_set(P_url(), P_host_header());
	get_addr(P_server(), P_port());
	init_epoll();
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, my_sighandler);
	epoll_loop();
	return 0;
}
