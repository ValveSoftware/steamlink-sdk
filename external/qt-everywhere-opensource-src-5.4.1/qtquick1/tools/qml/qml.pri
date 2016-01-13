QT += declarative declarative-private script network sql core-private gui-private widgets-private
qtHaveModule(opengl) {
    QT += opengl
    DEFINES += GL_SUPPORTED
}

INCLUDEPATH += $$PWD

HEADERS += $$PWD/qmlruntime.h \
           $$PWD/proxysettings.h \
           $$PWD/qdeclarativetester.h \
           $$PWD/deviceorientation.h \
           $$PWD/loggerwidget.h
SOURCES += $$PWD/qmlruntime.cpp \
           $$PWD/proxysettings.cpp \
           $$PWD/qdeclarativetester.cpp \
           $$PWD/loggerwidget.cpp

RESOURCES = $$PWD/browser/browser.qrc \
            $$PWD/startup/startup.qrc

linux-g++-maemo {
    QT += dbus
    SOURCES += $$PWD/deviceorientation_harmattan.cpp
    FORMS = $$PWD/recopts.ui \
            $$PWD/proxysettings.ui
} else {
    SOURCES += $$PWD/deviceorientation.cpp
    FORMS = $$PWD/recopts.ui \
            $$PWD/proxysettings.ui
}

