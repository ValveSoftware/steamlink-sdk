QT += core gui widgets webenginewidgets testlib

TARGET = inputmethods
TEMPLATE = app


SOURCES += \
    colorpicker.cpp \
    controlview.cpp \
    main.cpp \
    referenceview.cpp \
    testview.cpp \
    webview.cpp

HEADERS  += \
    colorpicker.h \
    controlview.h \
    referenceview.h \
    testview.h \
    webview.h

RESOURCES += \
    test.qrc

DISTFILES += \
    testdata.csv
