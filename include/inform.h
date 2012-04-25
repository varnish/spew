/* spew communication headers
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

#ifndef _COM_H
#define _COM_H

#include <stdio.h>

#include "verbosities.h"

/* Send information. v is the verbosity level as described below. */
#define inform(v, ...) inform_real(v, __func__, __FILE__, __LINE__, __VA_ARGS__)
void inform_real(const unsigned int v,
		 const char *func,
		 const char *file,
		 const unsigned int line, const char *fmt, ...);

/* Verify that inform() is ready and sanity-check the verbosity levels.
 * Also sets up a new file descriptor for log messages.
 */
void inform_init(FILE * fd);

/* Describe the p-verbosity level in human readable format on fd. If p is
 * -1, all verbosity levels are described.
 */
void inform_describe_verbosity(FILE * fd, const int p);

/* Passed to inform(): inform(V(XHANDLED),"foo") for instance. */
#define V(s) (1<<VER_ ## s)

#endif				// COM_H
