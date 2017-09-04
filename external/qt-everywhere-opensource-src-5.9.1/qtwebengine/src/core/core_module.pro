MODULE = webenginecore

include(core_common.pri)
# Needed to set a CFBundleIdentifier
QMAKE_INFO_PLIST = Info_mac.plist

linking_pri = $$OUT_PWD/$$getConfigDir()/$${TARGET}.pri

!include($$linking_pri) {
    error("Could not find the linking information that gn should have generated.")
}

load(qt_module)

api_library_name = qtwebenginecoreapi$$qtPlatformTargetSuffix()
api_library_path = $$OUT_PWD/api/$$getConfigDir()

# Do not precompile any headers. We are only interested in the linker step.
PRECOMPILED_HEADER =

isEmpty(NINJA_OBJECTS): error("Missing object files from QtWebEngineCore linking pri.")
isEmpty(NINJA_LFLAGS): error("Missing linker flags from QtWebEngineCore linking pri")
isEmpty(NINJA_ARCHIVES): error("Missing archive files from QtWebEngineCore linking pri")
isEmpty(NINJA_LIBS): error("Missing library files from QtWebEngineCore linking pri")
NINJA_OBJECTS = $$eval($$list($$NINJA_OBJECTS))
# Do manual response file linking for macOS and Linux

RSP_FILE = $$OUT_PWD/$$getConfigDir()/$${TARGET}.rsp
for(object, NINJA_OBJECTS): RSP_CONTENT += $$object
write_file($$RSP_FILE, RSP_CONTENT)
macos:LIBS_PRIVATE += -Wl,-filelist,$$shell_quote($$RSP_FILE)
linux:LIBS_PRIVATE += @$$RSP_FILE
# QTBUG-58710 add main rsp file on windows
win32:QMAKE_LFLAGS += @$$RSP_FILE
linux: LIBS_PRIVATE += -Wl,--start-group $$NINJA_ARCHIVES -Wl,--end-group
else: LIBS_PRIVATE += $$NINJA_ARCHIVES
LIBS_PRIVATE += $$NINJA_LIB_DIRS $$NINJA_LIBS
# GN's LFLAGS doesn't always work across all the Linux configurations we support.
# The Windows and macOS ones from GN does provide a few useful flags however
linux: QMAKE_LFLAGS += -Wl,--gc-sections -Wl,-O1 -Wl,-z,now -Wl,-z,defs
else: QMAKE_LFLAGS += $$NINJA_LFLAGS
POST_TARGETDEPS += $$NINJA_TARGETDEPS


LIBS_PRIVATE += -L$$api_library_path
CONFIG *= no_smart_library_merge
osx {
    LIBS_PRIVATE += -Wl,-force_load,$${api_library_path}$${QMAKE_DIR_SEP}lib$${api_library_name}.a
} else:msvc {
    QMAKE_LFLAGS += /OPT:REF
    QMAKE_LFLAGS += /WHOLEARCHIVE:$${api_library_path}$${QMAKE_DIR_SEP}$${api_library_name}.lib
} else {
    LIBS_PRIVATE += -Wl,-whole-archive -l$$api_library_name -Wl,-no-whole-archive
}

win32-msvc* {
    POST_TARGETDEPS += $${api_library_path}$${QMAKE_DIR_SEP}$${api_library_name}.lib
} else {
    POST_TARGETDEPS += $${api_library_path}$${QMAKE_DIR_SEP}lib$${api_library_name}.a
}

# Using -Wl,-Bsymbolic-functions seems to confuse the dynamic linker
# and doesn't let Chromium get access to libc symbols through dlsym.
CONFIG -= bsymbolic_functions

qtConfig(egl): CONFIG += egl

linux:qtConfig(separate_debug_info): QMAKE_POST_LINK="cd $(DESTDIR) && $(STRIP) --strip-unneeded $(TARGET)"

REPACK_DIR = $$OUT_PWD/$$getConfigDir()

# Duplicated from resources/resources.gyp
LOCALE_LIST = am ar bg bn ca cs da de el en-GB en-US es-419 es et fa fi fil fr gu he hi hr hu id it ja kn ko lt lv ml mr ms nb nl pl pt-BR pt-PT ro ru sk sl sr sv sw ta te th tr uk vi zh-CN zh-TW
for(LOC, LOCALE_LIST) {
    locales.files += $$REPACK_DIR/qtwebengine_locales/$${LOC}.pak
}
resources.files = $$REPACK_DIR/qtwebengine_resources.pak \
    $$REPACK_DIR/qtwebengine_resources_100p.pak \
    $$REPACK_DIR/qtwebengine_resources_200p.pak \
    $$REPACK_DIR/qtwebengine_devtools_resources.pak

icu.files = $$OUT_PWD/$$getConfigDir()/icudtl.dat

!debug_and_release|!build_all|CONFIG(release, debug|release) {
    qtConfig(framework) {
        locales.version = Versions
        locales.path = Resources/qtwebengine_locales
        resources.version = Versions
        resources.path = Resources
        icu.version = Versions
        icu.path = Resources
        # No files, this prepares the bundle Helpers symlink, process.pro will create the directories
        qtwebengineprocessplaceholder.version = Versions
        qtwebengineprocessplaceholder.path = Helpers
        QMAKE_BUNDLE_DATA += icu locales resources qtwebengineprocessplaceholder
    } else {
        locales.CONFIG += no_check_exist
        locales.path = $$[QT_INSTALL_TRANSLATIONS]/qtwebengine_locales
        resources.CONFIG += no_check_exist
        resources.path = $$[QT_INSTALL_DATA]/resources
        INSTALLS += locales resources

        !use?(system_icu) {
            icu.CONFIG += no_check_exist
            icu.path = $$[QT_INSTALL_DATA]/resources
            INSTALLS += icu
        }
    }

    !qtConfig(framework):!force_independent {
        #
        # Copy essential files to the qtbase build directory for non-prefix builds
        #

        !use?(system_icu) {
            COPIES += icu
        }

        COPIES += resources locales
    }
}

!build_pass:debug_and_release {
    # Special GNU make target that ensures linking isn't done for both debug and release builds
    # at the same time.
    notParallel.target = .NOTPARALLEL
    QMAKE_EXTRA_TARGETS += notParallel
}

OTHER_FILES = \
    $$files(../3rdparty/chromium/*.py, true) \
    $$files(../3rdparty/chromium/*.gyp, true) \
    $$files(../3rdparty/chromium/*.gypi, true) \
    $$files(../3rdparty/chromium/*.gn, true) \
    $$files(../3rdparty/chromium/*.gni, true)
