TARGET = qtwebenginecoreapi$$qtPlatformTargetSuffix()
DESTDIR = $$OUT_PWD/$$getConfigDir()

TEMPLATE = lib

CONFIG += staticlib c++11
QT += network core-private
QT_PRIVATE += webenginecoreheaders-private

# Don't create .prl file for this intermediate library because
# their contents get used when linking against them, breaking
# "-Wl,-whole-archive -lqtwebenginecoreapi --Wl,-no-whole-archive"
CONFIG -= create_prl

# Copy this logic from qt_module.prf so that the intermediate library can be
# created to the same rules as the final module linking in core_module.pro.
!host_build:if(win32|mac):!macx-xcode {
    qtConfig(debug_and_release): CONFIG += debug_and_release build_all
}

DEFINES += \
    BUILDING_CHROMIUM \
    NOMINMAX

CHROMIUM_SRC_DIR = $$QTWEBENGINE_ROOT/$$getChromiumSrcDir()
INCLUDEPATH += $$QTWEBENGINE_ROOT/src/core \
               $$CHROMIUM_SRC_DIR

linux-g++*: QMAKE_CXXFLAGS += -Wno-unused-parameter

HEADERS = \
    qwebenginecallback.h \
    qwebenginecallback_p.h \
    qtwebenginecoreglobal.h \
    qtwebenginecoreglobal_p.h \
    qwebenginecookiestore.h \
    qwebenginecookiestore_p.h \
    qwebengineurlrequestinterceptor.h \
    qwebengineurlrequestinfo.h \
    qwebengineurlrequestinfo_p.h \
    qwebengineurlrequestjob.h \
    qwebengineurlschemehandler.h

SOURCES = \
    qtwebenginecoreglobal.cpp \
    qwebenginecookiestore.cpp \
    qwebengineurlrequestinfo.cpp \
    qwebengineurlrequestjob.cpp \
    qwebengineurlschemehandler.cpp

msvc {
    # Create a list of object files that can be used as response file for the linker.
    # This is done to simulate -whole-archive on MSVC.
    QMAKE_POST_LINK = \
        "if exist $(DESTDIR_TARGET).objects del $(DESTDIR_TARGET).objects$$escape_expand(\\n\\t)" \
        "for %%a in ($(OBJECTS)) do echo $$shell_quote($$shell_path($$OUT_PWD))\\%%a >> $(DESTDIR_TARGET).objects"
}
