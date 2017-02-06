
INCLUDEPATH += video

PUBLIC_HEADERS += \
    video/qabstractvideobuffer.h \
    video/qabstractvideosurface.h \
    video/qvideoframe.h \
    video/qvideosurfaceformat.h \
    video/qvideoprobe.h \
    video/qabstractvideofilter.h

PRIVATE_HEADERS += \
    video/qabstractvideobuffer_p.h \
    video/qimagevideobuffer_p.h \
    video/qmemoryvideobuffer_p.h \
    video/qvideooutputorientationhandler_p.h \
    video/qvideosurfaceoutput_p.h \
    video/qvideoframe_p.h \
    video/qvideoframeconversionhelper_p.h

SOURCES += \
    video/qabstractvideobuffer.cpp \
    video/qabstractvideosurface.cpp \
    video/qimagevideobuffer.cpp \
    video/qmemoryvideobuffer.cpp \
    video/qvideoframe.cpp \
    video/qvideooutputorientationhandler.cpp \
    video/qvideosurfaceformat.cpp \
    video/qvideosurfaceoutput.cpp \
    video/qvideoprobe.cpp \
    video/qabstractvideofilter.cpp \
    video/qvideoframeconversionhelper.cpp

SSE2_SOURCES += video/qvideoframeconversionhelper_sse2.cpp
SSSE3_SOURCES += video/qvideoframeconversionhelper_ssse3.cpp
AVX2_SOURCES += video/qvideoframeconversionhelper_avx2.cpp
