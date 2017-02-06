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
    $$PWD/directshowcameraglobal.h \
    $$PWD/dscameraviewfindersettingscontrol.h \
    $$PWD/dscameraimageprocessingcontrol.h

SOURCES += \
    $$PWD/dscameraservice.cpp \
    $$PWD/dscameracontrol.cpp \
    $$PWD/dsvideorenderer.cpp \
    $$PWD/dsvideodevicecontrol.cpp \
    $$PWD/dsimagecapturecontrol.cpp \
    $$PWD/dscamerasession.cpp \
    $$PWD/dscameraviewfindersettingscontrol.cpp \
    $$PWD/dscameraimageprocessingcontrol.cpp

*-msvc*:INCLUDEPATH += $$(DXSDK_DIR)/include
QMAKE_USE += directshow
