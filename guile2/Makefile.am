# libchop -- a utility library for distributed storage and data backup
# Copyright (C) 2010, 2011  Ludovic Courtès <ludo@gnu.org>
#
# Libchop is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Libchop is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with libchop.  If not, see <http://www.gnu.org/licenses/>.

SOURCES =					\
  chop/core.scm					\
  chop/choppers.scm				\
  chop/internal.scm				\
  chop/objects.scm				\
  chop/stores.scm				\
  chop/streams.scm				\
  srfi/srfi-64.scm

SOURCES_NOT_COMPILED =				\
  srfi/srfi-64.upstream.scm

nobase_guilemodule_DATA = $(SOURCES) $(SOURCES_NOT_COMPILED)

if HAVE_GUILE2

GUILEC_FLAGS = -Wunbound-variable -Warity-mismatch -Wunused-toplevel -Wformat

# XXX: This expression relies on a GNU extension.
GOBJECTS = $(SOURCES:%.scm=%.go)

CLEANFILES = $(GOBJECTS)
nobase_nodist_guilemodule_DATA = $(GOBJECTS)

.scm.go:
	$(MKDIR_P) `dirname $@`
	libchop_libdir="$(abs_top_builddir)/src"				\
	libchop_includedir="$(abs_top_srcdir)/include"				\
	CC="$(CC)" LIBTOOL="$(LIBTOOL)"						\
	GUILE_AUTO_COMPILE=0							\
	GUILE_LOAD_COMPILED_PATH="$(builddir):$$GUILE_LOAD_COMPILED_PATH"	\
	guile-tools compile -L "$(srcdir)" $(GUILEC_FLAGS) -o "$@" "$<"

TESTS_ENVIRONMENT =							\
  libchop_libdir="$(abs_top_builddir)/src"				\
  GUILE_LOAD_COMPILED_PATH="$(builddir):$$GUILE_LOAD_COMPILED_PATH"	\
  $(GUILE) --no-auto-compile -L $(srcdir)

TESTS = test-suite.scm

else !HAVE_GUILE2

EXTRA_DIST = test-suite.scm

endif