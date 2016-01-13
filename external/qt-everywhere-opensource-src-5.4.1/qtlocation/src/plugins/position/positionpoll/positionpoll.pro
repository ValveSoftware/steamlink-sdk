TARGET = qtposition_positionpoll
QT = core positioning

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryPoll
load(qt_plugin)

SOURCES += \
    qgeoareamonitor_polling.cpp \
    positionpollfactory.cpp

HEADERS += \
    qgeoareamonitor_polling.h \
    positionpollfactory.h

INCLUDEPATH += $$QT.location.includes

OTHER_FILES += \
    plugin.json
