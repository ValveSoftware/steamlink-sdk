TEMPLATE = app
TARGET = customvideoitem

QT += multimedia multimediawidgets widgets

qtHaveModule(opengl): QT += opengl

HEADERS   += videoplayer.h \
             videoitem.h

SOURCES   += main.cpp \
             videoplayer.cpp \
             videoitem.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/multimediawidgets/customvideosurface/customvideoitem
INSTALLS += target
