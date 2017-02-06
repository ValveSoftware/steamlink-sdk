TARGET = qtsensors_iio-sensor-proxy
QT = core dbus sensors

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = IIOSensorProxySensorPlugin
load(qt_plugin)

!android:LIBS += -lrt
HEADERS += iiosensorproxysensorbase.h \
    iiosensorproxylightsensor.h \
    iiosensorproxyorientationsensor.h \
    iiosensorproxycompass.h

SOURCES += iiosensorproxysensorbase.cpp \
    iiosensorproxylightsensor.cpp \
    iiosensorproxyorientationsensor.cpp \
    iiosensorproxycompass.cpp \
    main.cpp

DBUS_INTERFACES += sensor_proxy sensor_proxy_compass dbus_properties

sensor_proxy.files = net.hadess.SensorProxy.xml
sensor_proxy.header_flags = -N

sensor_proxy_compass.files = net.hadess.SensorProxy.Compass.xml
sensor_proxy_compass.header_flags = -N

dbus_properties.files = org.freedesktop.DBus.Properties.xml
dbus_properties.header_flags = -N

OTHER_FILES = plugin.json $$DBUS_INTERFACES
