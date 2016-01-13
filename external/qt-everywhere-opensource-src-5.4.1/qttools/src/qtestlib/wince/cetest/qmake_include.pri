#qmake source files needed for cetest
HEADERS += \
           $$QT.core.sources/../../qmake/option.h
SOURCES += \
           $$QT.core.sources/../../qmake/option.cpp \
           $$QT.core.sources/../../qmake/project.cpp \
           $$QT.core.sources/../../qmake/property.cpp \
           $$QT.core.sources/../../qmake/generators/metamakefile.cpp \
           $$QT.core.sources/../../tools/shared/windows/registry.cpp

DEFINES += QT_QMAKE_PARSER_ONLY
