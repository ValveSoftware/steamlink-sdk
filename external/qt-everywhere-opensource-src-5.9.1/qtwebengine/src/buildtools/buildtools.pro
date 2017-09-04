TEMPLATE = subdirs

ninja.file = ninja.pro
SUBDIRS += ninja

gn.file = gn.pro
gn.depends = ninja
SUBDIRS += gn

linux {
    # configure_host.pro and configure_target.pro are phony pro files that
    # extract things like compiler and linker from qmake.
    # Only used on Linux as it is only important for cross-building and alternative compilers.
    configure_host.file = configure_host.pro
    configure_target.file = configure_target.pro
    configure_target.depends = configure_host
    gn.depends += configure_target

    SUBDIRS += configure_host configure_target
}
