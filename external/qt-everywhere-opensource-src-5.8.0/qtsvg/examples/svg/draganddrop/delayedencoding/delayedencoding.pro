QT          += widgets svg

HEADERS     = mimedata.h \
              sourcewidget.h
RESOURCES   = delayedencoding.qrc
SOURCES     = main.cpp \
              mimedata.cpp \
              sourcewidget.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/svg/draganddrop/delayedencoding
INSTALLS += target

simulator: warning(This example does not work on Simulator platform)
