INCLUDEPATH += camera

PRIVATE_HEADERS += \
    camera/qcamera_p.h

PUBLIC_HEADERS += \
    camera/qcamera.h \
    camera/qcameraimagecapture.h \
    camera/qcameraexposure.h \
    camera/qcamerafocus.h \
    camera/qcameraimageprocessing.h \
    camera/qcamerainfo.h

SOURCES += \
    camera/qcamera.cpp \
    camera/qcameraexposure.cpp \
    camera/qcamerafocus.cpp \
    camera/qcameraimageprocessing.cpp \
    camera/qcameraimagecapture.cpp \
    camera/qcamerainfo.cpp

