CXX_MODULE = multimedia
TARGET  = declarative_multimedia
TARGETPATH = QtMultimedia

QT += qml quick network multimedia-private qtmultimediaquicktools-private

HEADERS += \
        qdeclarativeaudio_p.h \
        qdeclarativemediametadata_p.h \
        qdeclarativeplaylist_p.h \
        qdeclarativeradio_p.h \
        qdeclarativeradiodata_p.h \
        qdeclarativecamera_p.h \
        qdeclarativecameracapture_p.h \
        qdeclarativecamerarecorder_p.h \
        qdeclarativecameraexposure_p.h \
        qdeclarativecameraflash_p.h \
        qdeclarativecamerafocus_p.h \
        qdeclarativecameraimageprocessing_p.h \
        qdeclarativecamerapreviewprovider_p.h \
        qdeclarativetorch_p.h \
        qdeclarativecameraviewfinder_p.h \
        qdeclarativemultimediaglobal_p.h

SOURCES += \
        multimedia.cpp \
        qdeclarativeaudio.cpp \
        qdeclarativeplaylist.cpp \
        qdeclarativeradio.cpp \
        qdeclarativeradiodata.cpp \
        qdeclarativecamera.cpp \
        qdeclarativecameracapture.cpp \
        qdeclarativecamerarecorder.cpp \
        qdeclarativecameraexposure.cpp \
        qdeclarativecameraflash.cpp \
        qdeclarativecamerafocus.cpp \
        qdeclarativecameraimageprocessing.cpp \
        qdeclarativecamerapreviewprovider.cpp \
        qdeclarativetorch.cpp \
        qdeclarativecameraviewfinder.cpp \
        qdeclarativemultimediaglobal.cpp

QML_FILES += \
    Video.qml

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

load(qml_plugin)
