INCLUDEPATH += $$PWD

PUBLIC_HEADERS += \
    $$PWD/qmacfunctions.h \

PRIVATE_HEADERS += $$PWD/qmacfunctions_p.h
OBJECTIVE_SOURCES += $$PWD/qmacfunctions.mm

LIBS_PRIVATE += -framework Foundation

uikit:!watchos {
    OBJECTIVE_SOURCES += \
        $$PWD/qmacfunctions_ios.mm

    LIBS_PRIVATE += -framework UIKit
}

macos {
    PUBLIC_HEADERS += \
    $$PWD/qmactoolbar.h \
    $$PWD/qmactoolbaritem.h \

    PRIVATE_HEADERS += \
        $$PWD/qmactoolbar_p.h \
        $$PWD/qmactoolbardelegate_p.h \
        $$PWD/qnstoolbar_p.h

    OBJECTIVE_SOURCES += \
        $$PWD/qmacfunctions_mac.mm \
        $$PWD/qmactoolbar.mm \
        $$PWD/qmactoolbaritem.mm \
        $$PWD/qmactoolbardelegate.mm \
        $$PWD/qnstoolbar.mm

    greaterThan(QT_MAJOR_VERSION, 4) {
        PUBLIC_HEADERS += $$PWD/qmacpasteboardmime.h
        OBJECTIVE_SOURCES += $$PWD/qmacpasteboardmime.mm
    }

    QT_PRIVATE += widgets
    LIBS_PRIVATE += -framework AppKit
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS
