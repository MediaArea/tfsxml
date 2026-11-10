AUTOMAKE_OPTIONS = foreign subdir-objects

lib_LTLIBRARIES = libtfsxml.la
libtfsxml_la_SOURCES = \
                       ../../../Source/tfsxml/tfsxml.c

libtfsxml_la_LDFLAGS     = -no-undefined -version-info 0:0:0

tfsxmlincludedir = $(includedir)/tfsxml
tfsxmlinclude_HEADERS = \
 ../../../Source/tfsxml/tfsxml.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libtfsxml.pc

if TINYXML2
lib_LTLIBRARIES += libtfsxml_tinyxml2.la
libtfsxml_tinyxml2_la_SOURCES = \
                       ../../../Source/tfsxml/tfsxml_tinyxml2.cpp
libtfsxml_tinyxml2_la_LIBADD = libtfsxml.la
libtfsxml_tinyxml2_la_LDFLAGS = -no-undefined -version-info 0:0:0
tfsxmlinclude_HEADERS += \
 ../../../Source/tfsxml/tfsxml_tinyxml2.h
endif

AM_TESTS_FD_REDIRECT = 9>&2

TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)
TESTS = test/regression.sh test/example.sh

check_PROGRAMS = test/RegressionTest test/HowToUse test/HowToUse_Stream
test_RegressionTest_SOURCES = \
                       ../../../Source/RegressionTest/RegressionTest.h \
                       ../../../Source/RegressionTest/RegressionTest.c \
                       ../../../Source/RegressionTest/RegressionTest_Api.c \
                       ../../../Source/RegressionTest/RegressionTest_Speed.c \
                       ../../../Source/RegressionTest/RegressionTest_Trace.c
test_RegressionTest_CFLAGS = $(AM_CFLAGS) -Wall -Wextra
test_RegressionTest_LDADD = libtfsxml.la
test_HowToUse_SOURCES = ../../../Source/Example/HowToUse.c
test_HowToUse_CFLAGS = $(AM_CFLAGS) -Wall -Wextra
test_HowToUse_LDADD = libtfsxml.la
test_HowToUse_Stream_SOURCES = ../../../Source/Example/HowToUse_Stream.c
test_HowToUse_Stream_CFLAGS = $(AM_CFLAGS) -Wall -Wextra
test_HowToUse_Stream_LDADD = libtfsxml.la

if TINYXML2
TESTS += test/tinyxml2.sh
check_PROGRAMS += test/RegressionTest_TinyXml2
test_RegressionTest_TinyXml2_SOURCES = \
                       ../../../Source/RegressionTest/RegressionTest_TinyXml2.h \
                       ../../../Source/RegressionTest/RegressionTest_TinyXml2.cpp
test_RegressionTest_TinyXml2_CXXFLAGS = $(AM_CXXFLAGS) -Wall -Wextra
test_RegressionTest_TinyXml2_LDADD = libtfsxml_tinyxml2.la libtfsxml.la
endif

EXTRA_DIST = test/regression.sh test/example.sh test/tinyxml2.sh

AM_CPPFLAGS = -I$(srcdir)/../../../Source
