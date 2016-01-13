QT += nfc widgets

CONFIG += strict_flags

TARGET = annotatedurl

SOURCES += main.cpp \
    mainwindow.cpp \
    annotatedurl.cpp

HEADERS  += mainwindow.h \
    annotatedurl.h

FORMS    += mainwindow.ui

target.path = $$[QT_INSTALL_EXAMPLES]/nfc/annotatedurl
INSTALLS += target
