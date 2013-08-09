/* spew - communication stuff
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

/* See inform.h for usage.
 *
 * The communication interface should be able to send information to a
 * generic file descriptor. At the moment, we're not quite there, but for
 * the purpose of using inform(), inform.c is feature-complete enough.
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include "param.h"
#include "inform.h"
#include "core.h"

/* Defines the various levels of verbosity we may or may not want. Position
 * and bit is both kept around even though they could be deduced from the
 * position and enum. This is to ensure that we can verify the integrity of
 * the structure easily.
 */
struct verbosity {
	const unsigned int position;
	const unsigned int bit;
	const char *name;
	const char *desc;
};

#include "verbosities.c"

/* fd of where to send information, typically stderr or some other funny
 * information channel.
 */
static FILE *i_output = NULL;

/* Make sure the fd is set to something besides NULL. This is mostly just a
 * hacked up init-thing.
 *
 * FIXME: This should be far smarter.
 */
static void com_check_fd(void)
{
	if (i_output == NULL)
		i_output = stderr;
	/*
	 * Should handle this more gracefully, in case we're hooked
	 * up to a socket.
	 */
	assert(!ferror(i_output));
}

/* Communicate the information and possibly where it came from
 * (function/file/line) if the verbosity dictates it.
 *
 * Output is sent to the fd set up by inform_init() assuming com_check_fd
 * doesn't reset it to stderr.
 *
 * Note that we do not distinguish between user and developer. All
 * information should be available upon request. There is no debug().
 */
void inform_real(const unsigned int v, const char *func, const char *file,
		 const unsigned int line, const char *fmt, ...)
{
	va_list ap;

	com_check_fd();

	if (spew.state == STATE_UNINIT || P_verbosity() & v) {
		if (P_verbosity() & V(FILELINE))
			fprintf(i_output, "0x%X:%s:%u: ", v, file, line);

		if (P_verbosity() & V(FUNCTION))
			fprintf(i_output, "%s(): ", func);

		va_start(ap, fmt);
		vfprintf(i_output, fmt, ap);
		va_end(ap);

		fprintf(i_output, "\n");
	}
}

/* Sanity-check of a verbosity-level.
 *
 * XXX: Since verbosity-levels aren't really prone to breaking, this is a
 * 	developer-failsafe more than anything. Also catches mismatches
 * 	between t_verbosity_enum and verbosity[].
 */
static void inform_verify_verbosity(int p)
{
	assert(verbosity[p].position == p);
	assert(verbosity[p].bit == 1 << p);
	assert(verbosity[p].desc);
	assert(verbosity[p].name);
}

/* Sets up fd as the new place to send information. Returns true if the
 * switch was successful, or the new and old fd is the same.
 *
 * XXX: A bit extensive due to inform(), but since we need to be
 * 	very explicit if inform() is likely to fail, this is
 * 	important.
 *
 * XXX: What to do with the old one?
 *
 */
static int inform_set_fd(FILE * fd)
{
	int ret;

	if (i_output == fd)
		return 1;

	if (fd == NULL) {
		inform(V(CORE),
		       "Attempted to switch to a NULL-pointer "
		       "for information messages. Ignoring it.");
		return 0;
	}

	ret = fileno(fd);
	if (ret == -1) {
		inform(V(CORE), "Refusing to switch logging to a bad fd");
		return 0;
	}
	ret = ferror(fd);
	if (ret) {
		inform(V(CORE),
		       "Attempted to switch file "
		       "descriptor for inform() to one with errors."
		       "Ignoring the change and using the old fd. "
		       "ferror() returned %d, new fd:%d. "
		       " now using: %d", ret, fileno(fd), fileno(i_output));
		if (fileno(fd) == -1)
			inform(V(CORE), "New fd is -1. Errno is: %d (%s)",
			       errno, strerror(errno));
	} else {
		i_output = fd;
		if (STATE_IS(CONFIGURED))
			inform(V(CORE),
			       "Switched to new file descriptor for logging.");
	}
	return 1;
}

/*
 * Basic sanity checks of the verbosities available to us.
 */
static void inform_assert_verbosities(void)
{
	int i;
	for (i = 0; i < VER_NUM; i++) {
		inform_verify_verbosity(i);
		if (strlen(verbosity[i].desc) < 10) {
			inform(V(CORE),
			       "Description of verbosity level %s "
			       "(%d, 0x%X) is alarmingly short "
			       "someone should punch the author!",
			       verbosity[i].name, i, verbosity[i].bit);
		}
	}
}

/* Set up whatever is needed to inform the user and verify that verbosity
 * is complete and sane while we're at it.
 *
 * XXX: Probably not very close to the final result, but a reasonable
 * 	place-holder nevertheless.
 */
void inform_init(FILE * fd)
{
	int ret;
	ret = inform_set_fd(fd);
	assert(ret);
	inform_assert_verbosities();

}

void inform_describe_verbosity(FILE * fd, const int p)
{
	int i;

	if (p == VER_ALL) {
		for (i = 0; i < VER_NUM; i++)
			inform_describe_verbosity(fd, i);
		return;
	}
	inform_verify_verbosity(p);

	fprintf(fd, "0x%X\t1<<%d\t\t%s\n", verbosity[p].bit,
		verbosity[p].position, verbosity[p].name);
	fprintf(fd, "%s\n\n", verbosity[p].desc);
	return;
}
