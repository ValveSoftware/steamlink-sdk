TEMPLATE = app
TARGET = videographicsitem

QT += multimedia multimediawidgets

HEADERS   += videoplayer.h

SOURCES   += main.cpp \
             videoplayer.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/multimediawidgets/videographicsitem
INSTALLS += target

QT+=widgets
