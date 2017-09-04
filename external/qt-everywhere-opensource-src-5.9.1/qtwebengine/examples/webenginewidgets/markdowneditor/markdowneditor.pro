TEMPLATE = app

QT += webenginewidgets webchannel
CONFIG += c++11

HEADERS += \
    mainwindow.h \
    previewpage.h \
    document.h

SOURCES = \
    main.cpp \
    mainwindow.cpp \
    previewpage.cpp \
    document.cpp

RESOURCES = \
    resources/markdowneditor.qrc

FORMS += \
    mainwindow.ui

DISTFILES += \
    3RDPARTY.md

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webenginewidgets/markdowneditor
INSTALLS += target
