MODULE = webenginecore

include(core_common.pri)
# Needed to set a CFBundleIdentifier
QMAKE_INFO_PLIST = Info_mac.plist

# Look for linking information produced by gyp for our target according to core_generated.gyp
!include($$OUT_PWD/$$getConfigDir()/$${TARGET}_linking.pri) {
    error("Could not find the linking information that gyp should have generated.")
}

load(qt_module)

api_library_name = qtwebenginecoreapi$$qtPlatformTargetSuffix()
api_library_path = $$OUT_PWD/api/$$getConfigDir()

# Do not precompile any headers. We are only interested in the linker step.
PRECOMPILED_HEADER =

LIBS_PRIVATE += -L$$api_library_path
CONFIG *= no_smart_library_merge
osx {
    LIBS_PRIVATE += -Wl,-force_load,$${api_library_path}$${QMAKE_DIR_SEP}lib$${api_library_name}.a
} else:msvc {
    # Simulate -whole-archive by passing the list of object files that belong to the public
    # API library as response file to the linker.
    QMAKE_LFLAGS += /OPT:REF
    QMAKE_LFLAGS += @$${api_library_path}$${QMAKE_DIR_SEP}$${api_library_name}.lib.objects
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

REPACK_DIR = $$OUT_PWD/$$getConfigDir()/gen/repack
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

!win32:!build_pass:debug_and_release {
    # Special GNU make target that ensures linking isn't done for both debug and release builds
    # at the same time.
    notParallel.target = .NOTPARALLEL
    QMAKE_EXTRA_TARGETS += notParallel
}

OTHER_FILES = \
    $$files(../3rdparty/chromium/*.h, true) \
    $$files(../3rdparty/chromium/*.cc, true) \
    $$files(../3rdparty/chromium/*.mm, true) \
    $$files(../3rdparty/chromium/*.py, true) \
    $$files(../3rdparty/chromium/*.gyp, true) \
    $$files(../3rdparty/chromium/*.gypi, true)
