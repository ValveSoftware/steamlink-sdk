load(qfeatures)
requires(!contains(QT_DISABLED_FEATURES, textedit))

TARGET     = QtScriptTools
QT         = core-private
QT_PRIVATE = gui widgets-private script

DEFINES   += QT_NO_USING_NAMESPACE
#win32-msvc*|win32-icc:QMAKE_LFLAGS += /BASE:0x66000000

QMAKE_DOCS = $$PWD/doc/qtscripttools.qdocconf

include(debugging/debugging.pri)

load(qt_module)
