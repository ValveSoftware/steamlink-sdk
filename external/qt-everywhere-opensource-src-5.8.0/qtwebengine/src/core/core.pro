TEMPLATE = subdirs

# core_headers is a dummy module to syncqt the headers so we can
# use them by later targets
core_headers.file = core_headers.pro

# core_gyp_generator.pro is a dummy .pro file that is used by qmake
# to generate our main .gyp file
core_gyp_generator.file = core_gyp_generator.pro
core_gyp_generator.depends = core_headers

# gyp_run.pro calls gyp through gyp_qtwebengine on the qmake step, and ninja on the make step.
gyp_run.file = gyp_run.pro
gyp_run.depends = core_gyp_generator

core_api.file = api/core_api.pro
core_api.depends = gyp_run

# This will take the compile output of ninja, and link+deploy the final binary.
core_module.file = core_module.pro
core_module.depends = core_api

SUBDIRS += core_headers \
           core_gyp_generator

!win32 {
    # gyp_configure_host.pro and gyp_configure_target.pro are phony pro files that
    # extract things like compiler and linker from qmake
    # Do not use them on Windows, where Qt already expects the toolchain to be
    # selected through environment varibles.
    gyp_configure_host.file = gyp_configure_host.pro
    gyp_configure_target.file = gyp_configure_target.pro
    gyp_configure_target.depends = gyp_configure_host

    gyp_run.depends += gyp_configure_host gyp_configure_target
    SUBDIRS += gyp_configure_host gyp_configure_target
}

SUBDIRS += gyp_run \
           core_api \
           core_module
