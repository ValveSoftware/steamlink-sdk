TARGET = QtRepParser
MODULE = repparser

# Dont produce a library
CONFIG += header_module

load(qt_module)

def_build = debug
contains(QT_CONFIG, release, debug|release): def_build = release
!debug_and_release|!build_all|CONFIG($$def_build, debug|release) {
    lib_bundle {
        # Make sure we install parser.g to the respective Frameworks include path if required
        FRAMEWORK_DATA.version = Versions
        FRAMEWORK_DATA.files = $$PWD/parser.g
        FRAMEWORK_DATA.path = Headers
        QMAKE_BUNDLE_DATA += FRAMEWORK_DATA
    } else {
        extra_headers.files = $$PWD/parser.g
        extra_headers.path = $$[QT_INSTALL_HEADERS]/$$MODULE_INCNAME
        !prefix_build {
            !equals(_PRO_FILE_PWD_, $$OUT_PWD): COPIES += extra_headers
        } else {
            INSTALLS += extra_headers
        }
    }
}

PUBLIC_HEADERS += \
    $$PWD/qregexparser.h

HEADERS += $$PUBLIC_HEADERS
