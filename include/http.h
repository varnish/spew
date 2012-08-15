/* spew http-headers
 * Copyright (C) 2009 Kristian Lyngstøl <kristian@bohemians.org>
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

#ifndef _SPEWHTTP_H
#define _SPEWHTTP_H

/*
 * Fugly
 */
#define MAX_EVENTS 1000
#define MAX_FDS 10000000

extern struct data_engine_t batch_data_engine;
extern struct data_engine_t rand_data_engine;

struct data_engine_t {
	void (*init)(struct data_engine_t *de, char *url, char *host);
	int (*write)(struct data_engine_t *de, int fd);
	void (*read)(struct data_engine_t *de, int fd);
	void *priv;
};


struct spew_eng_t {
	int fds[MAX_FDS];
	int epollfd;
	struct addrinfo *result;
	struct data_engine_t *data_engine;
};

int http_main(void);
int (*data_writer)(int fd);
#endif
