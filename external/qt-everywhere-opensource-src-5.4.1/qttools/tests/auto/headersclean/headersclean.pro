QT = core help designer
load(qt_headersclean)
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
# shadowing problems in help, allow it for now
gcc: QMAKE_CXXFLAGS -= -Wshadow
