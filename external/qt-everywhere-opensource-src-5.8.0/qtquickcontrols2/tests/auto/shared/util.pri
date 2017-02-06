QT += core-private gui-private qml-private quick-private quicktemplates2-private

HEADERS += $$PWD/visualtestutil.h \
           $$PWD/util.h
SOURCES += $$PWD/visualtestutil.cpp \
           $$PWD/util.cpp

DEFINES += QT_QMLTEST_DATADIR=\\\"$${_PRO_FILE_PWD_}/data\\\"
