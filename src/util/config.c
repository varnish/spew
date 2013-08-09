/* spew - config parsing
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

#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <wordexp.h>

#include "param.h"
#include "inform.h"
#include "core.h"

/*
 * The actual configuration file.
 */
static FILE *configfd = NULL;

/*
 * Where we are in the configuration file (meh, cheap hack).
 */
static int config_line = 0;

/*
 * Buffer increments when parsing an option.
 */
#define CONFIG_BUFFER_INCREMENT	1024

/*
 * buf: actual buffer
 * size: how large it is BEFORE it's multipled with sizeof()
 * pos: where we are currently adding data.
 */
struct config_buffer {
	char *buf;
	size_t size;
	int pos;
};

/*
 * Opens a config file as defined by P_config, using shell-like expansion.
 *
 * FIXME: Should eventually use XDG base dirs. No rush, ~/.config/spew is
 * 	  usually XDG basedir-compliant.
 */
static int config_open(void)
{
	char *d;
	wordexp_t p;
	char **w;
	int ret = 0;

	assert(configfd == NULL);
	d = P_config();
	assert(d);
	if (*d == '\0') {
		inform(V(CONFIG),
		       "Configuration file is blank. "
		       " Resetting it to default. Use /dev/null for "
		       "a blank configuration.");
		param_set_default(PARAM_config, P_STATE_DEFAULT);
		d = P_config();
		assert(d);
		assert(*d != '\0');
	}

	wordexp(d, &p, 0);
	w = p.we_wordv;
	assert(p.we_wordc == 1);
	inform(V(CONFIG), "Configuration file: %s", w[0]);
	configfd = fopen(w[0], "r");

	if (configfd == NULL) {
		inform(V(CORE), "Unable to read config file `%s': %s",
		       w[0], strerror(errno));
		if (errno == ENOENT) {
			inform(V(CORE),
			       "Defaults are used instead."
			       " See --help param to generate a configuration file.");
			ret = 1;
		} else {
			ret = 0;
		}
		goto out;
	}
	ret = 1;
	config_line = 1;
 out:
	wordfree(&p);
	return ret;
}

/*
 * XXX: Probably want to handle this more gracefully, though I don't really
 *	know how this could fail.
 */
static void config_close(void)
{
	int freturn;

	assert(configfd);
	freturn = fclose(configfd);
	assert(freturn == 0);
}

/*********************************************************************
 * Basic config file buffers                                         *
 *********************************************************************/

/*
 * config_start_buffer sets up the initial buffer at
 * CONFIG_BUFFER_INCREMENT size,
 *
 * confing_stop_buffer frees up.
 *
 * config_add_buf() adds a character to the buffer, expanding it if
 * necessary.
 *
 * config_expand_buf() increments the buffer size by
 * CONFIG_BUFFER_INCREMENT size.
 *
 * config_purge_buf adds a null-character to the buffer and asks param.c to
 * parse it, then resets the buffer-position (not size).
 *
 * so:
 *  start_buffer.
 *  read non-comment stuff and add it with add_buf()
 *  when EOL, EOF or a comment is reached, run config_purge_buf()
 *  when EOF is reached: stop_buffer.
 */
static void config_start_buffer(struct config_buffer *buf)
{
	assert(buf->buf == NULL);
	assert(buf->size == 0);
	buf->size = CONFIG_BUFFER_INCREMENT;
	buf->buf = malloc(buf->size * sizeof(char));
	assert(buf->buf);
	buf->pos = 0;
}

static void config_stop_buffer(struct config_buffer *buf)
{
	assert(buf);
	assert(buf->buf);
	free(buf->buf);
	buf->pos = 0;
	buf->size = 0;
}

/*
 * Expands the configuration-file buffer, if necessary.
 */
static void config_expand_buf(struct config_buffer *buf)
{
	assert(buf);
	assert(buf->buf);
	buf->size += CONFIG_BUFFER_INCREMENT;
	buf->buf = realloc(buf->buf, buf->size * sizeof(char));
	assert(buf->buf);
}

/*
 * Adds the c to the buffer, expanding it if necessary.
 */
static void config_add_buf(struct config_buffer *buf, int c)
{
	char ch = (unsigned char)c;
	assert(c != EOF);
	assert(buf);
	assert(buf->buf);
	if (buf->size <= buf->pos)
		config_expand_buf(buf);
	buf->buf[buf->pos] = ch;
	buf->pos++;
}

/*
 * Sends the content of the buffer to be parsed by param_parse, then resets
 * the position of the buffer.
 */
static int config_purge_buf(struct config_buffer *buf)
{
	assert(buf);
	if (buf->pos == 0)
		return 1;

	config_add_buf(buf, '\0');
	if (!param_parse(buf->buf, P_STATE_CONFIG)) {
		inform(V(CORE),
		       "Failed to parse the configuration file "
		       "at line %d: %s", config_line, buf->buf);
		return 0;
	}
	buf->pos = 0;
	return 1;
}

/*
 * Basic read of the config file. Really needs a sanity check...
 *
 * Markup in this context: 
 * - If #: Mark as comment
 * - If \n: unmark as comment
 * - If \n and NOT longline: parse the buffer with param_parse()
 * - If \n and longline: Add the \n to the buffer.
 * - If {: Increase longline
 * - If { AND longline: Add { to buffer.
 * - If }: Decrease longline
 * - If } and longline < 0: Fail (no matching {)
 * - If } and longline: Add } to buffer.
 * - If EOF AND longline: Warn of unmatched { and fail
 * - If EOF and NOT longline: Parse buffer with param_parse() and end.
 *
 * Note that param.c handles actual mapping of the parameter, this is just
 * for the configuration file. So we do not handle =, for instance here.
 * (We may want to add a non-= feature for things like booleans, for
 * instance, who knows - it's not within the scope of config.c to
 * determine the correct syntax for what a parameter is - just where the
 * definition starts and stops).
 *
 * XXX: I want a clearer way to describe this process. Suggestions?
 *
 * XXX: This should probably be split up, the complexity is a tad too
 * 	high for my liking. -K
 */
static int config_read(void)
{
	int ret = 0;
	int c;
	int comment = 0;
	int longline = 0;
	int longwhere = 0;
	struct config_buffer buf = { NULL, 0 };
	config_start_buffer(&buf);

	assert(configfd);

	do {
		c = fgetc(configfd);
		switch (c) {
		case EOF:
			if (longline) {
				inform(V(CONFIG),
				       "Reached end of config "
				       "file without closing `}'. "
				       "Opening { was at line %d", longwhere);
				ret = 0;
				goto out;
			}
			if (config_purge_buf(&buf))
				ret = 1;
			else
				ret = 0;
			goto out;
		case '{':
			if (longline)
				config_add_buf(&buf, c);
			else {
				longwhere = config_line;
			}
			longline++;
			break;
		case '}':
			longline--;
			if (longline < 0) {
				inform(V(CONFIG),
				       "Found `}' without "
				       "previously matching `{' on line %d:",
				       config_line);
				ret = 0;
				goto out;
			}
			if (longline)
				config_add_buf(&buf, c);
			break;
			/*
			 * XXX: Comments are stripped for longlines too... Is this
			 *      wise?
			 */
		case '#':
			if (!longline && !config_purge_buf(&buf)) {
				ret = 0;
				goto out;
			}
			comment = 1;
			break;
		case '\n':
			if (longline)
				config_add_buf(&buf, c);
			else if (!config_purge_buf(&buf)) {
				ret = 0;
				goto out;
			}
			config_line++;
			comment = 0;
			break;
		default:
			if (!comment)
				config_add_buf(&buf, c);
			break;
		}
	} while (c != EOF);
 out:
	config_stop_buffer(&buf);
	return ret;
}

int config_init(void)
{
	if (STATE_IS(CONFIGURED))
		set_state(RECONFIGURE);

	if (!config_open())
		return 0;

	/*
	 * config_open returns true if file was not found.
	 */
	if (!configfd)
		return 1;
	if (!config_read())
		return 0;
	config_close();
	if (STATE_IS(CONFIGURED))
		unset_state(RECONFIGURE);
	set_state(CONFIGURED);
	return 1;
}
