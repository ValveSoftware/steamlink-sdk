TEMPLATE = app
TARGET = satelliteinfo

QT += quick positioning

SOURCES += main.cpp \
    satellitemodel.cpp

HEADERS += \
    satellitemodel.h

OTHER_FILES += \
    satelliteinfo.qml

RESOURCES += \
    satelliteinfo.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/qtpositioning/satelliteinfo
INSTALLS += target



