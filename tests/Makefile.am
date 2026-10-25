# Unit tests for the pure layout math in src/mga_layout.c.
# No Xorg headers or hardware required; runs from `make check`.

AM_CPPFLAGS = -I$(top_srcdir)/src

check_PROGRAMS = test_layout
TESTS = test_layout

test_layout_SOURCES = test_layout.c \
	$(top_srcdir)/src/mga_layout.c \
	$(top_srcdir)/src/mga_layout.h
