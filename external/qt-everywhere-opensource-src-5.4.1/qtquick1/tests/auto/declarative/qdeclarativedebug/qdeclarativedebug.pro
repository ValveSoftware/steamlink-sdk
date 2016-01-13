CONFIG += testcase
TARGET = tst_qdeclarativedebug

QT += testlib network declarative declarative-private core-private script-private gui-private widgets-private
macx:CONFIG -= app_bundle

HEADERS += ../shared/debugutil_p.h
SOURCES += tst_qdeclarativedebug.cpp \
           ../shared/debugutil.cpp

CONFIG += parallel_test declarative_debug

CONFIG+=insignificant_test # QTBUG-26702
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
