HEADERS         = qdbusviewer.h \
                  qdbusmodel.h \
                  servicesproxymodel.h \
                  propertydialog.h \
                  logviewer.h \
                  mainwindow.h

SOURCES         = qdbusviewer.cpp \
                  qdbusmodel.cpp \
                  servicesproxymodel.cpp \
                  propertydialog.cpp \
                  logviewer.cpp \
                  mainwindow.cpp \
                  main.cpp \

RESOURCES += qdbusviewer.qrc

QT += widgets dbus-private xml

mac {
    ICON = images/qdbusviewer.icns
    QMAKE_INFO_PLIST = Info_mac.plist
}

win32 {
    RC_FILE = qdbusviewer.rc
}

load(qt_app)
