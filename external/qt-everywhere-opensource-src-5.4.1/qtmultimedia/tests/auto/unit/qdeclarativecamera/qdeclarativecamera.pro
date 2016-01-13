TARGET = tst_qdeclarativecamera
CONFIG += warn_on qmltestcase

QT += multimedia-private

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockcamera.pri)

SOURCES += \
        tst_qdeclarativecamera.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0

OTHER_FILES += \
    tst_qdeclarativecamera.qml
