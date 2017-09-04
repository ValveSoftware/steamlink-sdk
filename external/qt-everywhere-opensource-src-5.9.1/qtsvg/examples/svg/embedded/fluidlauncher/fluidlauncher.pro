QT += widgets

VERSION = $$QT_VERSION

HEADERS += \
           demoapplication.h \
           fluidlauncher.h \
           pictureflow.h \
           slideshow.h

SOURCES += \
           demoapplication.cpp \
           fluidlauncher.cpp \
           main.cpp \
           pictureflow.cpp \
           slideshow.cpp

RESOURCES = fluidlauncher.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/svg/embedded/fluidlauncher
INSTALLS += target
