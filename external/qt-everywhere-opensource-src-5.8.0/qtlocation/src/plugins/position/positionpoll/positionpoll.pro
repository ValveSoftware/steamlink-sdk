TARGET = qtposition_positionpoll

QT = core positioning

SOURCES += \
    qgeoareamonitor_polling.cpp \
    positionpollfactory.cpp

HEADERS += \
    qgeoareamonitor_polling.h \
    positionpollfactory.h

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryPoll
load(qt_plugin)
