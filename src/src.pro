CONFIG += debug
CONFIG += warn_off
CONFIG += silent

QT_CONFIG-=no-pkg-config
CONFIG+=link_pkgconfig
PKGCONFIG=apr-1 apr-util-1
PKGCONFIG+=libsvn_fs libsvn_repos libsvn_subr

exists(local-config.pri):include(local-config.pri)

macos {
  CONFIG -= app_bundle
  CONFIG += sdk_no_version_check
  QMAKE_MACOSX_DEPLOYMENT_TARGET=12.0
  LIBS += -L$$system(brew --prefix)/lib
}

if(!defined(VERSION, var)) {
  VERSION = $$system(git --no-pager show --pretty=oneline --no-notes | head -1 | cut -b-40)
}

VERSTR = '\\"$${VERSION}\\"'  # place quotes around the version string
DEFINES += VER=\"$${VERSTR}\" # create a VER macro containing the version string

TEMPLATE = app
TARGET = svn-all-fast-export
DESTDIR = ../

isEmpty(PREFIX) {
    PREFIX = /usr/local
}
BINDIR = $$PREFIX/bin

INSTALLS += target
target.path = $$BINDIR

DEPENDPATH += .
QT = core

# Input
SOURCES += ruleparser.cpp \
    repository.cpp \
    svn.cpp \
    main.cpp \
    CommandLineParser.cpp \

HEADERS += ruleparser.h \
    repository.h \
    svn.h \
    CommandLineParser.h \
