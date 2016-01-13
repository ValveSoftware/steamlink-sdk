QT      += webkitwidgets network widgets

HEADERS = framecapture.h
SOURCES = main.cpp \
          framecapture.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webkitwidgets/framecapture
INSTALLS += target
