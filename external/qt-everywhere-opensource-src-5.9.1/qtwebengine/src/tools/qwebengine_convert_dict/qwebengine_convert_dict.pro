option(host_build)

# Look for linking information produced by gyp for our target according to core_generated.gyp
linking_pri = $$OUT_PWD/../../core/$$getConfigDir()/convert_dict.pri

!include($$linking_pri) {
    error("Could not find the linking information that gn should have generated.")
}

isEmpty(NINJA_OBJECTS): error("Missing object files from QtWebEngineCore linking pri.")
isEmpty(NINJA_LFLAGS): error("Missing linker flags from QtWebEngineCore linking pri")
isEmpty(NINJA_ARCHIVES): error("Missing archive files from QtWebEngineCore linking pri")
isEmpty(NINJA_LIBS): error("Missing library files from QtWebEngineCore linking pri")
OBJECTS = $$eval($$list($$NINJA_OBJECTS))
linux {
    LIBS_PRIVATE = -Wl,--start-group $$NINJA_ARCHIVES -Wl,--end-group
} else {
    LIBS_PRIVATE = $$NINJA_ARCHIVES
}
LIBS_PRIVATE += $$NINJA_LIB_DIRS $$NINJA_LIBS
QMAKE_LFLAGS += $$NINJA_LFLAGS
POST_TARGETDEPS += $$NINJA_TARGETDEPS

#ninja compiles with std::__debug
linux: CONFIG(debug, debug|release) {
    DEFINES += _GLIBCXX_DEBUG
}

# Fixme: -Werror=unused-parameter in core
QMAKE_CXXFLAGS_WARN_ON =

# Disable MSVC2015 warning C4577 ('noexcept' used with no exception handling mode specified)
win32: QMAKE_CXXFLAGS_WARN_ON = -wd4577

# Issue with some template compliation, smb smart should look at it
win32: DEFINES += NOMINMAX

CHROMIUM_SRC_DIR = $$QTWEBENGINE_ROOT/$$getChromiumSrcDir()
INCLUDEPATH += $$CHROMIUM_SRC_DIR

SOURCES += \
    main.cpp

# Support converting dictionaries in a prefix build, by supplying
# the path to the ICU data file located in the Qt build path, rather
# than the install path (which is not present at build time).
icu_data_dir.name = QT_WEBENGINE_ICU_DATA_DIR
icu_data_dir.value = $$OUT_PWD/../../../src/core/$$getConfigDir()
QT_TOOL_ENV = icu_data_dir
load(qt_tool)
QT_TOOL_ENV =
