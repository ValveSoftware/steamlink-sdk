# --- The Find widget is also linked into the designer_shared library.
#     Avoid conflict when linking statically
contains(CONFIG, static) {
    INCLUDEPATH *= ../../../../shared/findwidget
} else {
    include(../../../../shared/findwidget/findwidget.pri)
}

INCLUDEPATH += $$PWD

HEADERS += $$PWD/objectinspector.h \
    $$PWD/objectinspectormodel_p.h \
    $$PWD/objectinspector_global.h

SOURCES += $$PWD/objectinspector.cpp \
    $$PWD/objectinspectormodel.cpp
