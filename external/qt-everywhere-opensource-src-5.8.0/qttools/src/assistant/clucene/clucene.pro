MODULE = clucene

TARGET = QtCLucene
QT = core

include(fulltextsearch.pri)

CONFIG += warn_off
contains(QT_CONFIG, reduce_exports) {
    CONFIG += hide_symbols
    # workaround for compiler errors on Ubuntu
    linux*-g++*:DEFINES += _GLIBCXX_EXTERN_TEMPLATE=0
}

# impossible to disable exceptions in clucene atm
CONFIG += exceptions

# otherwise mingw headers do not declare common functions like _i64tow
win32-g++*:QMAKE_CXXFLAGS_CXX11 = -std=gnu++0x

msvc {
    QMAKE_CFLAGS_RELEASE        -= -O2
    QMAKE_CXXFLAGS_RELEASE      -= -O2
    DEFINES += _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
}

# the following define could be set globally in case we need it elsewhere
solaris* {
    DEFINES += Q_SOLARIS_VERSION=$$system(uname -r | sed -e 's/5\\.//')
}

CONFIG += internal_module
load(qt_module)
