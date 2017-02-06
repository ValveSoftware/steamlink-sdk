QT += widgets
DEPENDPATH += $$PWD

HEADERS += \
    $$PWD/qwidgetplatform_p.h \
    $$PWD/qwidgetplatformcolordialog_p.h \
    $$PWD/qwidgetplatformdialog_p.h \
    $$PWD/qwidgetplatformfiledialog_p.h \
    $$PWD/qwidgetplatformfontdialog_p.h \
    $$PWD/qwidgetplatformmenu_p.h \
    $$PWD/qwidgetplatformmenuitem_p.h \
    $$PWD/qwidgetplatformmessagedialog_p.h \
    $$PWD/qwidgetplatformsystemtrayicon_p.h

SOURCES += \
    $$PWD/qwidgetplatformcolordialog.cpp \
    $$PWD/qwidgetplatformdialog.cpp \
    $$PWD/qwidgetplatformfiledialog.cpp \
    $$PWD/qwidgetplatformfontdialog.cpp \
    $$PWD/qwidgetplatformmenu.cpp \
    $$PWD/qwidgetplatformmenuitem.cpp \
    $$PWD/qwidgetplatformmessagedialog.cpp \
    $$PWD/qwidgetplatformsystemtrayicon.cpp
