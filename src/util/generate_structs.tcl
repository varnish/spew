#!/usr/bin/tclsh8.4
# spew - Parameter and verbosity-level generation
# Copyright (C) 2010 Kristian Lyngstol <kristian@bohemians.org>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
###
# Generates various .h and .c files for parameters. One place to edit
# instead of two (four).
#
# Handles: verbosities.{c,h} param-list.{c,h}

# Parameter type families.
set tfamilies { simple string key }

# Used to generate the param_type_id enum: available parameter types.
set types {
	{bool simple}
	{uint simple}
	{int simple}
	{mask simple}
	{string string}
	{key key}
	}

# Parameters.
#
# TCL-syntax means {} and "" is roughly the same. The indentation is a bit
# funky here to avoid sliding the descriptions too far to the right.
#
# format is {name type default <min max> {description}}
# min/max is only valid for types where it makes sense, and otherwise
# invalid. In other words: integers.

set params {
	{url		string	 "/" 		{ "The URL to ask for." }}
	{server		string	"localhost" 	{ "Target server to connect to." }}
	{port		string	"80"	 	{ "Port number to connect to." }}
	{host_header	string	"localhost" 	{ "Host header to use." }}
	{rand			INT	0	0	100000000 {
		"If set, adds ?int to the URLs where int is a number"
		"between 0 and the number specified. E.g: randomize=5 will"
		"create the following requests:"
		""
		"GET /?0"
		"GET /?1"
		"GET /?2"
		"GET /?3"
		"GET /?4"
		""
		"Used to create distinct URLs."
		"If left at 0, no ? is added."
		"Note that spew currently just has one set of URLs. If you"
		"set rand to a value higher than reqs, it will effectively"
		"be truncated."
	}}
	{timer_threshold	INT	10000	1	100000000 {
		"Threshold (in microseconds) for when to report slow"
		"operations (HTTP_DEBUG verbosity)."
	}}
	{conns		INT	100	1	100000 {
		"Number of connections to keep open concurrently."
	}}
	{reqs		INT	100	1	100000 {
		"Number of requests to send through a connection"
		"before reconnecting"
	}}
	{snd_buff	INT	1048576000	8192	1048576000 {
		"Send buffer"
	}}
	{rcv_buff	INT	1048576000	8192	1048576000 {
		"Receive buffer"
	}}
	{config		string	 "~/.config/spew" {
		"The location of the configuration file"
		""
		"Does nothing in a file, but can be overridden by -p"
	}}
	{verbosity	MASK
	 {(UINT_MAX ^ ((1<<VER_FILELINE|(1<<VER_STATE))))} {
		"Bit-mask deciding how verbose spew should be"
		""
		"See --help verbosity for a list of possibilities"
		""
		"Cheat sheet: 0: display nothing. -1: Display everything"
	}}
}

# Levels of verbosity.
#
# Keep in mind: changing the order changes the config-syntax.
# IE: Don't do it. Add to the bottom.
set verbosities {
	{CORE		"Core information. You almost always want this."}
	{STATE		"Changes in state: Connect/disconnect/handling"}
	{HTTP_INFO	"HTTP info."}
	{HTTP_DEBUG	"HTTP debug info."}
	{HTTP_CRIT	"HTTP critical info."}
	{CONFIG_CHANGES "Configuration changes"}
	{CONFIG		"Parsing/verification of configuration"}
	{NOTIMPLEMENTED "Warnings when unimplemented functions are called"}
	{FILELINE	"Include source-file and line number in output"}
	{FUNCTION	"Include the calling function-name in the output"}
}

#############################################################
# Actual parsing starts here. Normally no need to modify it.#
#############################################################

# Print the boilerplate-warning to avoid confusing people.
proc warn {fd} {
	puts $fd "/*"
	puts $fd " * Automatically generated from gen_objects.tcl."
	puts $fd " *"
	puts $fd " * Do not modify. Modify src/generate_structs.tcl instead"
	puts $fd " */"
	puts $fd ""
}

# The header-file for paramters
set head [open "../include/param-list.h" w]

warn $head
set n 0
puts $head "enum param_type_id {"
foreach type $types {
	puts -nonewline $head "\tPTYPE_[string toupper [lindex $type 0]]"
	if {$n == 0} {
		puts -nonewline $head " = 0"
	}
	incr n
	puts $head ","
}
puts $head "\tPTYPE_NUM"
puts $head "};"
puts $head ""

set n 0
puts $head "enum param_id {"
foreach param $params {
	puts -nonewline $head "\tPARAM_[string tolower [lindex $param 0]]"
	if {$n == 0} {
		puts -nonewline $head " = 0"
	}
	incr n
	puts $head ","
}

puts $head "\tPARAM_NUM,"
# P_ALL is used to signal to a function to iterate all parameters
puts $head "\tPARAM_ALL"
puts $head "};"
puts $head ""

# Actual data structure for parameters
set c [open "../include/param-list.c" w]
warn $c

puts $c "
/*
 * Everything done by param.c on behalf of other modules is verified, so
 * explicitly telling param.c to verify something is redundant and thus not
 * exposed.
 */
"
foreach fam $tfamilies {
	puts $c "static param_set_func ptype_set_${fam};"
	puts $c "static param_print_func ptype_print_${fam};"
	puts $c "static param_verify_func ptype_verify_${fam};"
	puts $c "static param_parse_func ptype_parse_${fam};"
	puts $c ""
}
puts $c "
/*
 * Different param-types we support.
 *
 * A \"family\" shares a common set of functions, but the individual
 * functions may distinguish between data types.
 *
 * set should free old values and allocate resources as needed.
 *
 * print should print just the value specified
 *
 * verify should sanity check that the data is within the expected
 * confines. It can safely ignore min/max where relevant, but should verify
 * that the min/max isn't set if it is ignored
 *
 * parse should be able to back out. Avoid assert() based on user input -
 * encourage it based on stupid code (ie: NULL-data).
 */
#define PA(name, family) 					\\
	\[PTYPE_ ## name\] = {					\\
		PTYPE_ ## name, #name,				\\
		ptype_set_ ## family,				\\
		ptype_print_ ## family, 			\\
		ptype_verify_ ## family,			\\
		ptype_parse_ ## family }

static struct param_type ptype\[PTYPE_NUM\] = {
"
foreach type $types {
	puts $c "PA([string toupper [lindex $type 0]], [lindex $type 1]),"
}

puts $c "};"
puts $c "#undef PA"
puts $c "
/*
 * Ensures that the parameter is mapped to the right enum.
 *
 * __VA_ARGS__ is a bit hacky right now. Better suggestions?
 */
"

puts $c "
#define PD(name,type,def,field,min,max, ...) 	\\
	\[PARAM_ ## name\] =				\\
	{ #name, PTYPE_ ## type, 0,		\\
	{ .v = NULL },				\\
	{ .field = def }, min, max , 		\\
	{ __VA_ARGS__ , NULL } },
"
puts $c "
/*
 * Actual settings matching the param enum in param.h
 *
 * Arguments:
 * name (P_name must exist in param.h)
 * type (See param-private.h, or the list underneath)
 * default value
 * field-name in the data-struct
 * minimum value (int)
 * maximum value (int)
 * description (separate lines with commas for formatting)
 *
 * XXX: Note that the min/max are not necessarily used, it depends on the
 * 	type of parameter.
 *
 * FIXME: The min/max should probably be printed by the type, to avoid
 * 	showing -1 as the max for MASK, for instance.
 *
 * FIXME: the field-name shouldn't be necessary.
 */
"
puts $c "static struct param param\[PARAM_NUM\] = {"

set n 0
foreach param $params {
	set name [lindex $param 0]
	set type [string toupper [lindex $param 1]]
	set def [string toupper [lindex $param 2]]
	set min 0
	set max 0
	set bf ""
	if {$n != 0} {
		puts $c ""
	}
	incr n
	set desc [lindex $param 3]
	if {$type == "BOOL"} {
		set min 0
		set max 1
		if {$def == "TRUE"} {
			set def "1"
		} elseif {$def == "FALSE"} {
			set def "0"
		} else {
			puts "Error: Boolean type with non-boolean default: ${name}"
			exit 1
		}
		set bf "b"
	} elseif {$type == "STRING"} {
		set min 0
		set max 65565
		if {$def != "NULL"} {
			set def "\"[lindex $param 2]\""
		}
		set bf "str"
	} elseif {$type == "MASK"} {
		set min 0
		set max "UINT_MAX"
		set bf "ul"
	} elseif {$type == "INT" || $type == "UINT" } {
		set min [lindex $param 3]
		set max [lindex $param 4]
		if {$type == "INT"} {
			set bf "i"
		} else {
			set bf "u"
			if {$min < 0} {
				puts "Parameter ${name} defined unsigned but with min<0."
				exit 1
			}
		}
		if {$def < $min || $def > $max} {
			puts "Parameter ${name} defined with default outside of valid range."
			exit 1
		}

		set desc [lindex $param 5]

	} else {
		puts "Unsupported param-type: ${type}."
		exit 1
	}
	puts $c "PD(${name}, ${type}, ${def}, ${bf}, ${min}, ${max}, "
	set mn 0
	foreach com $desc {
		if {$mn != 0} {
			puts $c ","
		}
		incr mn
		puts -nonewline $c "\t\"${com}\""
	}
	puts -nonewline $c ")"
	puts $head "#define P_${name}() param_get(PARAM_${name}).${bf}"
#define P(i) param_get(PARAM_ ## i)
}
puts $c "};"
puts $c "#undef PD"

set verc [open "../include/verbosities.c" w]
warn $verc

puts $verc "
/* Short for \"Add Verbosity\" ... Or something like that. */
#define AV(name,desc) \\
	\{ VER_ ## name, 1<<VER_ ## name, #name, desc \}

/* Different verbosity levels, as defined in t_verbosity_enum. VER_ isn't
 * included as it's only needed internally. (V() and AV() both add it as
 * needed) 
 */
struct verbosity verbosity\[VER_NUM\] = \{"

set n 0

foreach ver $verbosities {
	if {$n != 0} {
		puts $verc ","
	}
	incr n
	puts -nonewline $verc "\tAV([lindex $ver 0], \"[lindex $ver 1]\")"
}
puts $verc ""
puts $verc "};"
puts $verc "#undef AV"


set verh [open "../include/verbosities.h" w]
warn $verh

puts $verh "
/* The types of verbosity available. The description is kept in com.c. This
 * should be reasonably opaque to the callers. Keep in mind that order
 * matters (verified by asserts).
 *
 * VER_ALL == all verbosities. Not used for bitmasks, just matching.
 */
enum verbosity_level {"

set n 0
foreach ver $verbosities {
	puts -nonewline $verh "\tVER_[lindex $ver 0]"
	if {$n == 0} {
		puts -nonewline $verh " = 0"
	}
	puts $verh ","
	incr n
}

puts $verh "\tVER_NUM,\n\tVER_ALL\n};"
