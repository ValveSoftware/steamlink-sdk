TARGET = qtposition_winrt

QT = core core-private positioning

SOURCES += qgeopositioninfosource_winrt.cpp \
    qgeopositioninfosourcefactory_winrt.cpp
HEADERS += qgeopositioninfosource_winrt_p.h \
    qgeopositioninfosourcefactory_winrt.h

OTHER_FILES += \
    plugin.json

PLUGIN_TYPE = position
PLUGIN_CLASS_NAME = QGeoPositionInfoSourceFactoryWinRT
msvc:!winrt: LIBS += runtimeobject.lib
load(qt_plugin)
