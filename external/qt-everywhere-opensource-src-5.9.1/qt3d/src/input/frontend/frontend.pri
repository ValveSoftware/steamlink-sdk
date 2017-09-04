HEADERS += \
    $$PWD/qinputaspect.h \
    $$PWD/qkeyboarddevice.h \
    $$PWD/qkeyboarddevice_p.h \
    $$PWD/qkeyboardhandler.h \
    $$PWD/qkeyboardhandler_p.h \
    $$PWD/qinputaspect_p.h \
    $$PWD/qkeyevent.h \
    $$PWD/qmousedevice.h \
    $$PWD/qmousedevice_p.h \
    $$PWD/qmousehandler.h \
    $$PWD/qmousehandler_p.h \
    $$PWD/qmouseevent.h \
    $$PWD/qinputdeviceplugin_p.h \
    $$PWD/qactioninput.h \
    $$PWD/qaction.h \
    $$PWD/qabstractaxisinput.h \
    $$PWD/qaxis.h \
    $$PWD/qanalogaxisinput.h \
    $$PWD/qbuttonaxisinput.h \
    $$PWD/qlogicaldevice.h \
    $$PWD/qinputdeviceintegration_p.h \
    $$PWD/qinputdeviceintegration_p_p.h \
    $$PWD/qabstractphysicaldevice.h \
    $$PWD/qinputdeviceintegrationfactory_p.h \
    $$PWD/qaxissetting.h \
    $$PWD/qabstractphysicaldevice_p.h \
    $$PWD/qgenericinputdevice_p.h \
    $$PWD/qabstractactioninput.h \
    $$PWD/qabstractactioninput_p.h \
    $$PWD/qinputchord.h \
    $$PWD/qinputsequence.h \
    $$PWD/qinputsettings.h \
    $$PWD/qinputsettings_p.h \
    $$PWD/qaction_p.h \
    $$PWD/qactioninput_p.h \
    $$PWD/qaxis_p.h \
    $$PWD/qabstractaxisinput_p.h \
    $$PWD/qanalogaxisinput_p.h \
    $$PWD/qbuttonaxisinput_p.h \
    $$PWD/qlogicaldevice_p.h \
    $$PWD/qaxissetting_p.h \
    $$PWD/qinputsequence_p.h \
    $$PWD/qinputchord_p.h \
    $$PWD/qphysicaldevicecreatedchange.h \
    $$PWD/qphysicaldevicecreatedchange_p.h \
    $$PWD/qaxisaccumulator.h \
    $$PWD/qaxisaccumulator_p.h \
    $$PWD/qabstractphysicaldeviceproxy_p.h \
    $$PWD/qabstractphysicaldeviceproxy_p_p.h


SOURCES += \
    $$PWD/qinputaspect.cpp \
    $$PWD/qkeyboarddevice.cpp \
    $$PWD/qkeyboardhandler.cpp \
    $$PWD/qkeyevent.cpp \
    $$PWD/qmousehandler.cpp \
    $$PWD/qmousedevice.cpp \
    $$PWD/qmouseevent.cpp \
    $$PWD/qinputdeviceplugin.cpp \
    $$PWD/qactioninput.cpp \
    $$PWD/qaction.cpp \
    $$PWD/qabstractaxisinput.cpp \
    $$PWD/qaxis.cpp \
    $$PWD/qanalogaxisinput.cpp \
    $$PWD/qbuttonaxisinput.cpp \
    $$PWD/qlogicaldevice.cpp \
    $$PWD/qinputdeviceintegration.cpp \
    $$PWD/qabstractphysicaldevice.cpp \
    $$PWD/qinputdeviceintegrationfactory.cpp \
    $$PWD/qaxissetting.cpp \
    $$PWD/qgenericinputdevice.cpp \
    $$PWD/qabstractactioninput.cpp \
    $$PWD/qinputchord.cpp \
    $$PWD/qinputsequence.cpp \
    $$PWD/qinputsettings.cpp \
    $$PWD/qphysicaldevicecreatedchange.cpp \
    $$PWD/qaxisaccumulator.cpp \
    $$PWD/qabstractphysicaldeviceproxy.cpp

qtHaveModule(gamepad) {
    QT += gamepad
    DEFINES += HAVE_QGAMEPAD
    HEADERS += $$PWD/qgamepadinput_p.h
    SOURCES += $$PWD/qgamepadinput.cpp
}

INCLUDEPATH += $$PWD

