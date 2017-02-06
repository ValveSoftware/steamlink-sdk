INCLUDEPATH += $$PWD

# --- Property browser is also linked into the designer_shared library.
#     Avoid conflict when linking statically
contains(CONFIG, static) {
    INCLUDEPATH *= ../../../../shared/qtpropertybrowser
    INCLUDEPATH *= ../../../../shared/qtgradienteditor
} else {
    include(../../../../shared/qtpropertybrowser/qtpropertybrowser.pri)
    include(../../../../shared/qtgradienteditor/qtcolorbutton.pri)
}

FORMS += $$PWD/paletteeditor.ui \
    $$PWD/stringlisteditor.ui \
    $$PWD/previewwidget.ui \
    $$PWD/newdynamicpropertydialog.ui

HEADERS += $$PWD/propertyeditor.h \
    $$PWD/designerpropertymanager.h \
    $$PWD/paletteeditor.h \
    $$PWD/paletteeditorbutton.h \
    $$PWD/stringlisteditor.h \
    $$PWD/stringlisteditorbutton.h \
    $$PWD/previewwidget.h \
    $$PWD/previewframe.h \
    $$PWD/newdynamicpropertydialog.h \
    $$PWD/brushpropertymanager.h \
    $$PWD/fontpropertymanager.h

SOURCES += $$PWD/propertyeditor.cpp \
    $$PWD/designerpropertymanager.cpp \
    $$PWD/paletteeditor.cpp \
    $$PWD/paletteeditorbutton.cpp \
    $$PWD/stringlisteditor.cpp \
    $$PWD/stringlisteditorbutton.cpp \
    $$PWD/previewwidget.cpp \
    $$PWD/previewframe.cpp \
    $$PWD/newdynamicpropertydialog.cpp \
    $$PWD/brushpropertymanager.cpp \
    $$PWD/fontpropertymanager.cpp

HEADERS += \
    $$PWD/propertyeditor_global.h \
    $$PWD/qlonglongvalidator.h

SOURCES += $$PWD/qlonglongvalidator.cpp

RESOURCES += $$PWD/propertyeditor.qrc
