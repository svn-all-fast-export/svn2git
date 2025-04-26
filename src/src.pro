if(!defined(SVN_INCLUDE, var)) {
  SVN_INCLUDE = /usr/include/subversion-1 /usr/local/include/subversion-1
}
if(!defined(APR_INCLUDE, var)) {
  APR_INCLUDE = /usr/include/apr-1.0 /usr/include/apr-1 /usr/local/include/apr-1
}
exists(local-config.pri):include(local-config.pri)

if(!defined(VERSION, var)) {
  VERSION = $$system(git --no-pager show --pretty=oneline --no-notes | head -1 | cut -b-40)
}

DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x05FFFF

VERSTR = '\\"$${VERSION}\\"'  # place quotes around the version string
DEFINES += VER=\"$${VERSTR}\" # create a VER macro containing the version string

TEMPLATE = app
TARGET = ../svn-all-fast-export

isEmpty(PREFIX) {
    PREFIX = /usr/local
}
BINDIR = $$PREFIX/bin

INSTALLS += target
target.path = $$BINDIR

DEPENDPATH += .
QT = core

greaterThan(QT_MAJOR_VERSION, 5) {
    QT += core5compat
}

INCLUDEPATH += . $$SVN_INCLUDE $$APR_INCLUDE
!isEmpty(SVN_LIBDIR): LIBS += -L$$SVN_LIBDIR
LIBS += -lsvn_fs-1 -lsvn_repos-1 -lapr-1 -lsvn_subr-1

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
