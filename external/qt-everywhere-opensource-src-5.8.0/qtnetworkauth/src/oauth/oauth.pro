TARGET = QtNetworkAuth
MODULE = networkauth

QT += core core-private network

PUBLIC_HEADERS += \
    qoauth1.h \
    qoauthglobal.h \
    qabstractoauth.h \
    qabstractoauth2.h \
    qoauth1signature.h \
    qoauthoobreplyhandler.h \
    qabstractoauthreplyhandler.h \
    qoauth2authorizationcodeflow.h \
    qoauthhttpserverreplyhandler.h

PRIVATE_HEADERS += \
    qoauth1_p.h \
    qabstractoauth_p.h \
    qabstractoauth2_p.h \
    qoauth1signature_p.h \
    qoauth2authorizationcodeflow_p.h \
    qoauthhttpserverreplyhandler_p.h

SOURCES += \
    qoauth1.cpp \
    qabstractoauth.cpp \
    qabstractoauth2.cpp \
    qoauth1signature.cpp \
    qoauthoobreplyhandler.cpp \
    qabstractoauthreplyhandler.cpp \
    qoauth2authorizationcodeflow.cpp \
    qoauthhttpserverreplyhandler.cpp

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

QMAKE_DOCS = $$PWD/doc/qtnetworkauth.qdocconf

load(qt_module)
