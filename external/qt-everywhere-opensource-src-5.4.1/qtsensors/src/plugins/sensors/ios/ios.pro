TARGET = qtsensors_ios
QT = core sensors

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = IOSSensorPlugin
load(qt_plugin)

OTHER_FILES = plugin.json

HEADERS += iosaccelerometer.h \
        iosmotionmanager.h \
        iosgyroscope.h \
        iosmagnetometer.h \
        ioscompass.h \
        iosproximitysensor.h

OBJECTIVE_SOURCES += main.mm \
    iosaccelerometer.mm \
    iosmotionmanager.mm \
    iosgyroscope.mm \
    iosmagnetometer.mm \
    ioscompass.mm \
    iosproximitysensor.mm

LIBS += -framework UIKit -framework CoreMotion -framework CoreLocation
