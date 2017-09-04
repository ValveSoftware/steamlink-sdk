TARGET = tst_qdeclarativecamera
CONFIG += warn_on qmltestcase

QT += multimedia-private

include (../qmultimedia_common/mock.pri)
include (../qmultimedia_common/mockcamera.pri)

SOURCES += \
        tst_qdeclarativecamera.cpp


OTHER_FILES += \
    tst_qdeclarativecamera.qml
