TARGET = qttcime

CONFIG += static

SOURCES += \
    cangjiedictionary.cpp \
    cangjietable.cpp \
    phrasedictionary.cpp \
    worddictionary.cpp \
    zhuyindictionary.cpp \
    zhuyintable.cpp

HEADERS += \
    cangjiedictionary.h \
    cangjietable.h \
    phrasedictionary.h \
    worddictionary.h \
    zhuyindictionary.h \
    zhuyintable.h

OTHER_FILES += \
    data/dict_cangjie.dat \
    data/dict_phrases.dat

MODULE_INCLUDEPATH = $$PWD

load(qt_helper_lib)

CONFIG += qt
QT = core
