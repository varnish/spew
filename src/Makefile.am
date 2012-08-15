AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror

bin_PROGRAMS = spew
spew_SOURCES = 	util/main.c \
		util/param.c \
		util/inform.c \
		util/arg.c \
		util/config.c \
		http.c \
		data_writers.c

spew_LDADD = 


${top_srcdir}/include/param-list.c: util/generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ util/generate_structs.tcl

${top_srcdir}/include/param-list.h: util/generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ util/generate_structs.tcl

${top_srcdir}/include/verbosities.c: util/generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ util/generate_structs.tcl

${top_srcdir}/include/verbosities.h: util/generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ util/generate_structs.tcl

util/main.c: ${top_srcdir}/include/param-list.c
