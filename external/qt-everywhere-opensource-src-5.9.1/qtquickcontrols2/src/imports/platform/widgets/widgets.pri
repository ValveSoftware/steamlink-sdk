QT += widgets
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/qwidgetplatform_p.h

qtConfig(systemtrayicon) {
    HEADERS += \
        $$PWD/qwidgetplatformsystemtrayicon_p.h
    SOURCES += \
        $$PWD/qwidgetplatformsystemtrayicon.cpp
}

qtConfig(colordialog) | qtConfig(filedialog) | qtConfig(fontdialog) | qtConfig(messagebox) {
    HEADERS += \
        $$PWD/qwidgetplatformdialog_p.h
    SOURCES += \
        $$PWD/qwidgetplatformdialog.cpp
}

qtConfig(colordialog) {
    HEADERS += \
        $$PWD/qwidgetplatformcolordialog_p.h
    SOURCES += \
        $$PWD/qwidgetplatformcolordialog.cpp
}

qtConfig(filedialog) {
    HEADERS += \
        $$PWD/qwidgetplatformfiledialog_p.h
    SOURCES += \
        $$PWD/qwidgetplatformfiledialog.cpp
}

qtConfig(fontdialog) {
    HEADERS += \
        $$PWD/qwidgetplatformfontdialog_p.h
    SOURCES += \
        $$PWD/qwidgetplatformfontdialog.cpp
}

qtConfig(menu) {
    HEADERS += \
        $$PWD/qwidgetplatformmenu_p.h \
        $$PWD/qwidgetplatformmenuitem_p.h
    SOURCES += \
        $$PWD/qwidgetplatformmenu.cpp \
        $$PWD/qwidgetplatformmenuitem.cpp
}

qtConfig(messagebox) {
    HEADERS += \
        $$PWD/qwidgetplatformmessagedialog_p.h
    SOURCES += \
        $$PWD/qwidgetplatformmessagedialog.cpp
}
