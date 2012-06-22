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
- Native IPv6 support (it was easier than not to have it!)

As an example of speed, I was able to do 260-290 thousand requests per
second against Varnish with spew using less than 10% cpu on my workstation
at home (granted, that's a fast computer). On my laptop (i5 M520, 2.4GHz),
spew easily does 30k-50k req/s with 3k-5k conn/s against a Varnish-server
on localhost.

Version history
===============

* 0.4 - Adds ``-p rand=<int>`` option to provide a random factor to urls.
  Minor build tweaks.
* 0.3 - Initial released version, sort of. 0.1, 0.2 and 0.3 were released
  on the same day.

Tips
====

- If you want to test more than 1000-ish connections, you need to
  increase the ulimit for number of file descriptors.

- Spew is relatively slow at opening connections. Or rather, opening
  connections is a slow task in general.

- If you open connections too fast, you may want to set::

        sysctl net.ipv4.tcp_tw_recycle=1

  Otherwise you'll quickly run out of local ports.

- Use ``spew -p url=/ -p otherparam=whatever -h param > ~/.config/spew`` to
  store your default options.

- Run multiple spew-processes if you starve a single CPU thread. Let me
  know if this ever speeds things up for you!

Installation and usage
======================

Dependencies: 
	
- C compiler
- make
- awk
- basic build stuff
- Linux (because of epoll)

From a repo: ./autogen.sh && ./configure && make && make install
From a tar-ball: ./configure && make && make install

Running: spew --help

If it's not in --help, it's not in spew. If it is, then someone got drunk,
wohoo!

See WIP (Work In Progress) for day-to-day changes. The content of WIP as it
was upon compile-time is usually printed upon startup.

Thanks
======

* Bearnard Hibbins <bearnard@gmail.com> -  Build instruction fixes
* Per Buer <perbu@varnish-software.com> - Feature requests and feedback

History
=======

The core part of spew, src/http.c, was hacked together in about 10 hours
total by Kristian Lyngstøl for Varnish Software.

Everything else was then later borrowed from an old defunct project I had
that only did parameter-parsin, config file support and all the typical
boring stuff. That's why http.c and the rest is pretty different.
If you find references to X-related stuff, let me know. They are old.
