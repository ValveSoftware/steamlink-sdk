INCLUDEPATH += $$PWD/evr

qtHaveModule(widgets): QT += widgets
QT += gui-private

LIBS += -lmf -lmfplat -lmfuuid -ld3d9 -ldxva2 -lwinmm -levr

HEADERS += \
    $$PWD/evr/evrvideowindowcontrol.h \
    $$PWD/evr/evrcustompresenter.h \
    $$PWD/evr/evrd3dpresentengine.h \
    $$PWD/evr/evrhelpers.h \
    $$PWD/evr/evrdefs.h

SOURCES += \
    $$PWD/evr/evrvideowindowcontrol.cpp \
    $$PWD/evr/evrcustompresenter.cpp \
    $$PWD/evr/evrd3dpresentengine.cpp \
    $$PWD/evr/evrhelpers.cpp \
    $$PWD/evr/evrdefs.cpp
