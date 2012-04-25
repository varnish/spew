AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS = -Wall -Werror

bin_PROGRAMS = spew
spew_SOURCES = main.c param.c inform.c arg.c config.c http.c
spew_LDADD = 


${top_srcdir}/include/param-list.c: generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ generate_structs.tcl

${top_srcdir}/include/param-list.h: generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ generate_structs.tcl

${top_srcdir}/include/verbosities.c: generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ generate_structs.tcl

${top_srcdir}/include/verbosities.h: generate_structs.tcl
	cd $(top_srcdir)/src/ && @TCLSH@ generate_structs.tcl

main.c: ${top_srcdir}/include/param-list.c
