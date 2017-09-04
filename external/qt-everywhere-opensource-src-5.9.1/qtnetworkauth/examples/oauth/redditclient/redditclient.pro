QT += widgets network networkauth

TARGET = redditclient

# Input
SOURCES += main.cpp \
    redditmodel.cpp \
    redditwrapper.cpp

HEADERS += \
    redditmodel.h \
    redditwrapper.h

# install
target.path = $$[QT_INSTALL_EXAMPLES]/oauth/redditclient
INSTALLS += target
