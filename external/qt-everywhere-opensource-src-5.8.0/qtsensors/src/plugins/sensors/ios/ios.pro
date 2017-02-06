TARGET = qtsensors_ios
QT = core sensors

OTHER_FILES = plugin.json

OBJECTIVE_SOURCES += main.mm
LIBS += -framework Foundation

uikit {
    ios {
        HEADERS += \
            ioscompass.h
        OBJECTIVE_SOURCES += \
            ioscompass.mm

        DEFINES += HAVE_COMPASS
        LIBS += -framework CoreLocation
    }

    !tvos {
        HEADERS += \
            iosaccelerometer.h \
            iosgyroscope.h \
            iosmagnetometer.h \
            iosmotionmanager.h
        OBJECTIVE_SOURCES += \
            iosaccelerometer.mm \
            iosgyroscope.mm \
            iosmagnetometer.mm \
            iosmotionmanager.mm

        DEFINES += HAVE_COREMOTION
        LIBS += -framework CoreMotion
    }

    !watchos {
        HEADERS += \
            iosproximitysensor.h
        OBJECTIVE_SOURCES += \
            iosproximitysensor.mm

        DEFINES += HAVE_UIDEVICE
        LIBS += -framework UIKit
    }
}

PLUGIN_TYPE = sensors
PLUGIN_CLASS_NAME = IOSSensorPlugin
load(qt_plugin)
