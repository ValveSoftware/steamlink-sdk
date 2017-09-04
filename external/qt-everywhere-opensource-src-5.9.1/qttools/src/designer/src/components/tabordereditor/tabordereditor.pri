
QT += xml

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/tabordereditor.h \
    $$PWD/tabordereditor_plugin.h \
    $$PWD/tabordereditor_tool.h \
    $$PWD/tabordereditor_global.h

SOURCES += \
    $$PWD/tabordereditor.cpp \
    $$PWD/tabordereditor_tool.cpp \
    $$PWD/tabordereditor_plugin.cpp

OTHER_FILES += $$PWD/tabordereditor.json
