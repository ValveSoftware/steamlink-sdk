TEMPLATE = subdirs

# core_gyp_generator.pro is a dummy .pro file that is used by qmake
# to generate our main .gyp file
core_gyp_generator.file = core_gyp_generator.pro

# gyp_run.pro calls gyp through gyp_qtwebengine on the qmake step, and ninja on the make step.
gyp_run.file = gyp_run.pro
gyp_run.depends = core_gyp_generator

# This will take the compile output of ninja, and link+deploy the final binary.
core_module.file = core_module.pro
core_module.depends = gyp_run

SUBDIRS += core_gyp_generator \
           gyp_run \
           core_module

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
