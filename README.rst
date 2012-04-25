===============
README for spew
===============

Spew is a HTTP request generator/spewer.

It's written to be fast and straight forward. It currently does no
measurements and is pretty mean. But it's also fast.

It has the following features:

- Generate HTTP requests to a server of your choice
- Manipulate the Host-header
- Tries to keep -p conns=X open connections at any given time
- Sends -p reqs=X requests over each connection - IN ONE GO.
- Ignores replies.
- It's fast. Or more accurately: It does nothing.
- It's single-threaded.

Tips
====

If you want to test more than 1000-ish connections, you need to
increase the ulimit for number of file descriptors.

Spew is relatively slow at opening multiple connections.

If you open connections too fast, you make want to set::

        sysctl net.ipv4.tcp_tw_recycle=1

Otherwise you'll quickly run out of local ports.

Use ``spew -p url=/ -p otherparam=whatever -h param > ~/.config/spew`` to store your
default options.

Installation and usage
======================

Dependencies: 
	
- C compiler
- make
- awk
- basic build stuff
- Linux (because of epoll)

From a repo: ./autogensh && ./configure && make && make install
From a tar-ball: ./configure && make && make install

Running: spew --help

If it's not in --help, it's not in spew. If it is, then someone got drunk,
wohoo!

See WIP (Work In Progress) for day-to-day changes. The content of WIP as it
was upon compile-time is usually printed upon startup.

History
=======

The core part of spew, src/http.c, was hacked together in about 10 hours
total by Kristian Lyngstøl for Varnish Software.

Everything else was then later borrowed from an old defunct project I had
that only did parameter-parsin, config file support and all the typical
boring stuff. That's why http.c and the rest is pretty different.
If you find references to X-related stuff, let me know. They are old.
