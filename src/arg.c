/* spew - argument handling
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
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#include "config.h"
#include "param.h"
#include "inform.h"
#include "core.h"

/* Getopt is a bit fugly....
 *
 * Thus: { "foo", 0, blatti, "hai" }, will point the blatti-variable to
 * "hai" if --foo is found...
 */
static struct option long_options[] = {
	{"help", optional_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"param", required_argument, 0, 'p'},
	{NULL}
};

/*
 * getopt() again. : == requires an argument. :: == optional 
 */
static char *short_options = "h::Vp:";

static void argv_version(FILE * fd)
{
	fprintf(fd, PACKAGE_STRING "\n");
	fprintf(fd, COPYRIGHT_STRING "\n");
	fprintf(fd, LICENSE_STRING "\n");
}

static int argv_param(char *arg)
{
	if (!param_parse(arg, P_STATE_ARGV)) {
		inform(V(CORE),
		       "Unable to parse a parameter specified "
		       "on the command line.");
		exit(1);
	}
	return 0;
}

static void argv_usage(FILE * fd)
{
	fprintf(fd, "Usage: spew [ options ... ]\n");
	fprintf(fd, " -V, --version\n\t\tprint the version of spew and exit.\n");
	fprintf(fd,
		" -h[subject], --help=subject\n\t\t"
		"prints generic help, or on the subject specified and exits\n"
		"\t\tValid subjects: param,paramlist,verbosity\n");
	fprintf(fd,
		" -p key=value, --param=k=v\n\t\t"
		"set the parameter key to value. Overrides configuration files.\n"
		"\t\tMultiple -p's can be specified\n");
	fprintf(fd, "\n");
}

static void argv_generic_help(FILE * fd)
{
	argv_usage(fd);
	fprintf(fd, "\nIf run without any parameters, spew cures cancer.\n");
	fprintf(fd,
		"That being said, read the individual parts of --help"
		" to learn more\n");
}

/* Print various types of help upon request.
 *
 * The style is a bit ugly, perhaps, but this should result in a nice
 * configuration file (among other things).
 *
 * XXX: strncmp is, perhaps, better?
 *
 * XXX: stdout is used for the param-stuff so it can be piped to a file
 * 	without inform()-noise if necessary.
 */
static void argv_help(char *arg)
{
	if (arg == NULL) {
		argv_generic_help(stdout);
	} else if (!strcmp(arg, "param")) {
/* *INDENT-OFF* */
		fprintf(stdout,
"# spew is configured by setting parameters.\n"
"#\n"
"# Parameters can be set in a configuration file or read on the\n"
"# command line. Command line takes priority over a configuration file.\n"
"#\n"
"# To explicitly set the value of a parameter to it's default, use:\n"
"#   url=default\n"
"#\n"
"# Parameter-names are case insensitive, white-space is generally stripped.\n"
"# In a configuration file, anything after a hash (#) is a comment.\n"
"#\n"
"# To make a clean configuration file, simply pipe this output to a file:\n"
"#   spew --help=param > .config/spew\n"
"#\n"
"# For just the parameter-name and value, use:\n"
"#   spew --help paramlist\n"
"#\n"
"# The following is the documentation on all the parameters available\n"
"# form spew. Either set them in a configuration file or override them with:\n"
"#    spew -p key=value -p otherkey=othervalue\n"
"#\n"
"# If the same value is supplied multiple times, the last one is used\n"
"#\n\n");
/* *INDENT-ON* */
		param_show(stdout, PARAM_ALL, P_WHAT_BIT(ALL));
	} else if (!strcmp(arg, "paramlist")) {
		param_show(stdout, PARAM_ALL,
			   P_WHAT_BIT(KEYVALUE) | P_WHAT_BIT(STATE_DEFAULTS));

	} else if (!strcmp(arg, "verbosity")) {
		inform_describe_verbosity(stdout, VER_ALL);
	} else if (*arg == '\0') {
		argv_generic_help(stdout);
	} else {
		inform(V(CORE), "--help without a valid argument.");
		argv_usage(stderr);
	}
}

/* 
 * Handle arguments, getopt()-style. May re-arrange argv. May also blow up.
 * Kaboom.
 */
int argv_init(int argc, char **argv)
{
	int c;

	while (1) {
		int option_index = 1;

		c = getopt_long(argc, argv, short_options, long_options,
				&option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			argv_help(optarg ? optarg : argv[optind]);
			exit(0);
			break;
		case 'V':
			argv_version(stdout);
			exit(0);
			break;
		case 'p':
			argv_param(optarg);
			break;
		default:
			argv_usage(stderr);
			exit(1);
			break;
		}
	}

	if (optind < argc) {
		inform(V(CORE), "Non-option argv elements found:");
		while (optind < argc)
			inform(V(CORE), "%s", argv[optind++]);
		argv_usage(stderr);
		exit(1);
	}
	return 1;
}
