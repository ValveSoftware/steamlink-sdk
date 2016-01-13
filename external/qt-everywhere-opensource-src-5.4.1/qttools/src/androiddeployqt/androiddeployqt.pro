option(host_build)
CONFIG += console

SOURCES += \
    main.cpp

# Required for declarations of popen/pclose on Windows
windows: QMAKE_CXXFLAGS += -U__STRICT_ANSI__

load(qt_app)

