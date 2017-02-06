
add_library(Qt5::IIOSensorProxySensorPlugin MODULE IMPORTED)

_populate_Sensors_plugin_properties(IIOSensorProxySensorPlugin RELEASE "sensors/libqtsensors_iio-sensor-proxy.so")

list(APPEND Qt5Sensors_PLUGINS Qt5::IIOSensorProxySensorPlugin)
