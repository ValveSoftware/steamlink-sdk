TEMPLATE=subdirs
qtHaveModule(widgets) {
    SUBDIRS = \
        qsvgdevice \
        qsvggenerator \
        qsvgrenderer \
        qsvgplugin \
        qicon_svg \
        cmake \
        installed_cmake

    installed_cmake.depends = cmake
}
!cross_compile: SUBDIRS += host.pro
