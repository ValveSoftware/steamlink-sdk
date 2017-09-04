TARGET = QtMultimedia
QT = core-private network gui-private

MODULE_PLUGIN_TYPES = \
    mediaservice \
    audio \
    video/bufferpool \
    video/gstvideorenderer \
    video/videonode \
    playlistformats \
    resourcepolicy

QMAKE_DOCS = $$PWD/doc/qtmultimedia.qdocconf

INCLUDEPATH *= .

PRIVATE_HEADERS += \
    qtmultimediaglobal_p.h \
    qmediacontrol_p.h \
    qmediaobject_p.h \
    qmediapluginloader_p.h \
    qmediaservice_p.h \
    qmediaserviceprovider_p.h \
    qmediaresourcepolicyplugin_p.h \
    qmediaresourcepolicy_p.h \
    qmediaresourceset_p.h \
    qmediastoragelocation_p.h \
    qmediaopenglhelper_p.h \
    qmultimediautils_p.h

PUBLIC_HEADERS += \
    qtmultimediaglobal.h \
    qmediabindableinterface.h \
    qmediacontrol.h \
    qmediaenumdebug.h \
    qmediametadata.h \
    qmediaobject.h \
    qmediaservice.h \
    qmediaserviceproviderplugin.h \
    qmediatimerange.h \
    qmultimedia.h

SOURCES += \
    qmediabindableinterface.cpp \
    qmediacontrol.cpp \
    qmediametadata.cpp \
    qmediaobject.cpp \
    qmediapluginloader.cpp \
    qmediaservice.cpp \
    qmediaserviceprovider.cpp \
    qmediatimerange.cpp \
    qmediaresourcepolicyplugin_p.cpp \
    qmediaresourcepolicy_p.cpp \
    qmediaresourceset_p.cpp \
    qmediastoragelocation.cpp \
    qmultimedia.cpp \
    qmultimediautils.cpp

CONFIG += simd optimize_full

include(audio/audio.pri)
include(camera/camera.pri)
include(controls/controls.pri)
include(playback/playback.pri)
include(radio/radio.pri)
include(recording/recording.pri)
include(video/video.pri)

ANDROID_BUNDLED_JAR_DEPENDENCIES = \
    jar/QtMultimedia-bundled.jar:org.qtproject.qt5.android.multimedia.QtMultimediaUtils
ANDROID_JAR_DEPENDENCIES = \
    jar/QtMultimedia.jar:org.qtproject.qt5.android.multimedia.QtMultimediaUtils
ANDROID_LIB_DEPENDENCIES = \
    plugins/mediaservice/libqtmedia_android.so \
    lib/libQt5MultimediaQuick_p.so:Qt5Quick
ANDROID_BUNDLED_FILES += \
    lib/libQt5MultimediaQuick_p.so
ANDROID_PERMISSIONS += \
    android.permission.CAMERA \
    android.permission.RECORD_AUDIO
ANDROID_FEATURES += \
    android.hardware.camera \
    android.hardware.camera.autofocus \
    android.hardware.microphone

MODULE_WINRT_CAPABILITIES_DEVICE += \
    microphone \
    webcam

win32: LIBS_PRIVATE += -luuid

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS

load(qt_module)
