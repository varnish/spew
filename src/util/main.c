/* HTTP Request spewer
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
#include "param.h"
#include "inform.h"
#include "core.h"
#include "WIP.h"
#include "http.h"

struct core spew;

/*
 * Only run this once, or mountain trolls may carry you off to the
 * wilderness.
 */
static void set_defaults(void)
{
	spew.state = 0;
	assert(param_set_default(PARAM_ALL, P_STATE_DEFAULT));
}

/*
 * See WIP for rationale.
 */
static void work_in_progress(void)
{
	int i;
	for (i = 0; WIP_list[i] != NULL; i++)
		inform(V(NOTIMPLEMENTED), "%s", WIP_list[i]);
}

/*
 * Let's keep it simple.
 */
int main(int argc, char **argv)
{
	int ret = 0;

	set_defaults();
	inform_init(stderr);
	argv_init(argc, argv);

	ret = config_init();
	assert(ret);

	work_in_progress();

	ret = http_main();
	return ret;
}
