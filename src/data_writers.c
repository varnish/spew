/* spew http engine - data writers
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "param.h"
#include "inform.h"
#include "core.h"
#include "http.h"


/*
 * BATCH
 */

struct batch_data_set_t {
	char *data;
	char *batch;
	size_t dsize;
	size_t bsize;
};


/*
 * Pre-creates the entire HTTP request. Petty dirty.
 *
 * It sticks Connection: close on the last request to trick the server into
 * doing out job for us. It's simpler.
 *
 * FIXME: UUUGH.
 */
static void batch_init(struct data_engine_t *de, char *url, char *host)
{
	struct batch_data_set_t *b_data; 
	char *buf = malloc(1024);
	char tmp[1024];
	char *batch;
	int i;
	int dec;
	b_data = malloc(sizeof (struct batch_data_set_t));
	de->priv = b_data;
	assert(b_data);
	assert(de->priv);
	assert(buf);
	assert(url);
	assert(host);
	assert(strlen(url) < 256);
	assert(strlen(host) < 256);
	if (P_rand() < P_reqs()) {
		inform(V(HTTP_INFO),"You're generating fewer requests"
				    "(reqs=) than your randomness setting"
				    "(rand=).");
		inform(V(HTTP_INFO),"rand= is effectively capped by reqs=");
	}
	if (P_rand() > 0) {
		snprintf(buf, 1024, "GET %s?%d HTTP/1.1\r\nHost: %s\r\n\r\n", url, P_rand()-1, host);
		snprintf(tmp, 1024,
			 "GET %s?%d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
			 url, P_rand()-1, host);
	} else {
		snprintf(buf, 1024, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", url, host);
		snprintf(tmp, 1024,
			 "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
			 url, host);
	}
	b_data->data = buf;
	b_data->dsize = strlen(buf);
	batch =
	    malloc(((P_reqs() - 1) * b_data->dsize) + P_reqs() + strlen(tmp));
	assert(batch);
	b_data->batch = batch;
	inform(V(HTTP_DEBUG), "Randomness: %d", P_rand());
	for (i = 0; i < (P_reqs() - 1); i++) {
		if (P_rand() > 0) {
			dec = snprintf(batch, 1024, "GET %s?%d HTTP/1.1\r\nHost: %s\r\n\r\n", url, i % P_rand(), host);
		} else {
			dec = snprintf(batch, 1024, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", url, host);
		}
		batch = batch + dec;
	}
	memcpy(batch, tmp, strlen(tmp));
	batch = batch + strlen(tmp);
	b_data->bsize = batch - b_data->batch;
}

/*
 * Writes the data needed. Not all that good...
 *
 * FIXME: Even if we do get that write failure, we have no way of
 *        recovering from it since we don't track the progress.... Hmm.
 *        Could probably use spew_eng.fds[] for that.
 */
static int batch_write(struct data_engine_t *de, int fd)
{
	int ret;
	struct batch_data_set_t *b_data = (struct batch_data_set_t *)(de->priv);
	ret = write(fd, b_data->batch, b_data->bsize);
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
static void batch_read(struct data_engine_t *de, int fd)
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
 * Random
 *
 * Deceptively similar to batch.
 */

struct rand_data_set_t {
	char *data;
	char *batch;
	char last_req[1024];
	char *url;
	char *host;
	size_t dsize;
	size_t bsize;
};


/*
 * Pre-creates the entire HTTP request. Petty dirty.
 *
 * It sticks Connection: close on the last request to trick the server into
 * doing out job for us. It's simpler.
 *
 * FIXME: UUUGH.
 */
static void rand_init(struct data_engine_t *de, char *url, char *host)
{
	struct rand_data_set_t *b_data;
	char *buf = malloc(1024);
	char *batch;
	b_data =  malloc(sizeof (struct rand_data_set_t));
	de->priv = (void *)b_data;
	assert(de->priv);
	assert(b_data);
	assert(buf);
	assert(url);
	assert(host);
	assert(strlen(url) < 256);
	assert(strlen(host) < 256);
	b_data->url = url;
	b_data->host = host;
	if (P_rand() < P_reqs()) {
		inform(V(HTTP_INFO),"You're generating fewer requests"
				    "(reqs=) than your randomness setting"
				    "(rand=).");
		inform(V(HTTP_INFO),"rand= is effectively capped by reqs=");
	}
	snprintf(buf, 1024, "GET %s%d HTTP/1.1\r\nHost: %s\r\n\r\n", url, P_rand()-1, host);
	snprintf(b_data->last_req, 1024,
		 "GET %s%d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
		 url, P_rand()-1, host);
	b_data->data = buf;
	b_data->dsize = strlen(buf);
	batch =
	    malloc(((P_reqs() - 1) * b_data->dsize) + P_reqs() + strlen(b_data->last_req));
	assert(batch);
	b_data->batch = batch;
	inform(V(HTTP_DEBUG), "Randomness: %d", P_rand());
}

/*
 * Generate individual requests.
 */
static void rand_make_req(struct rand_data_set_t *b_data) {
	char *batch = b_data->batch;
	int i,dec;
	for (i = 0; i < (P_reqs() - 1); i++) {
		dec = snprintf(batch, 1024, "GET %s%d HTTP/1.1\r\nHost: %s\r\n\r\n", b_data->url, (int)random() % P_rand(), b_data->host);
		batch = batch + dec;
	}
	memcpy(batch, b_data->last_req, strlen(b_data->last_req));
	batch = batch + strlen(b_data->last_req);
	b_data->bsize = batch - b_data->batch;
}

/*
 * Writes the data needed. Not all that good...
 *
 * FIXME: Even if we do get that write failure, we have no way of
 *        recovering from it since we don't track the progress.... Hmm.
 *        Could probably use spew_eng.fds[] for that.
 */
static int rand_write(struct data_engine_t *de, int fd)
{
	int ret;
	struct rand_data_set_t *b_data = (struct rand_data_set_t *)(de->priv);
	TIME(rand_make_req(b_data));
	ret = write(fd, b_data->batch, b_data->bsize);
	if (ret <= 0 && errno != EAGAIN) {
		inform(V(HTTP_CRIT), "Write failed. Errno: %d:%s", errno,
		       strerror(errno));
		return -1;
	}
	return 0;
}

struct data_engine_t batch_data_engine = { &batch_init, &batch_write, &batch_read, NULL};
struct data_engine_t rand_data_engine = { &rand_init, &rand_write, &batch_read, NULL};
