
INCLUDEPATH += video

PUBLIC_HEADERS += \
    video/qabstractvideobuffer.h \
    video/qabstractvideosurface.h \
    video/qvideoframe.h \
    video/qvideosurfaceformat.h \
    video/qvideoprobe.h

PRIVATE_HEADERS += \
    video/qabstractvideobuffer_p.h \
    video/qimagevideobuffer_p.h \
    video/qmemoryvideobuffer_p.h \
    video/qvideooutputorientationhandler_p.h \
    video/qvideosurfaceoutput_p.h

SOURCES += \
    video/qabstractvideobuffer.cpp \
    video/qabstractvideosurface.cpp \
    video/qimagevideobuffer.cpp \
    video/qmemoryvideobuffer.cpp \
    video/qvideoframe.cpp \
    video/qvideooutputorientationhandler.cpp \
    video/qvideosurfaceformat.cpp \
    video/qvideosurfaceoutput.cpp \
    video/qvideoprobe.cpp




