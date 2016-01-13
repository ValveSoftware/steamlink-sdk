INCLUDEPATH += $$PWD

DEFINES += QMEDIA_DIRECTSHOW_CAMERA

mingw: DEFINES += QT_NO_WMSDK

win32: DEFINES += _CRT_SECURE_NO_WARNINGS

HEADERS += \
    $$PWD/dscameraservice.h \
    $$PWD/dscameracontrol.h \
    $$PWD/dsvideorenderer.h \
    $$PWD/dsvideodevicecontrol.h \
    $$PWD/dsimagecapturecontrol.h \
    $$PWD/dscamerasession.h \
    $$PWD/directshowglobal.h

SOURCES += \
    $$PWD/dscameraservice.cpp \
    $$PWD/dscameracontrol.cpp \
    $$PWD/dsvideorenderer.cpp \
    $$PWD/dsvideodevicecontrol.cpp \
    $$PWD/dsimagecapturecontrol.cpp \
    $$PWD/dscamerasession.cpp

*-msvc*:INCLUDEPATH += $$(DXSDK_DIR)/include
LIBS += -lstrmiids -ldmoguids -luuid -lmsdmo -lole32 -loleaut32
