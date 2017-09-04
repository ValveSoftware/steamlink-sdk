TEMPLATE = subdirs
QT_FOR_CONFIG += waylandcompositor-private

qtConfig(wayland-egl): \
    SUBDIRS += wayland-egl
qtConfig(wayland-brcm): \
    SUBDIRS += brcm-egl
qtConfig(xcomposite-egl): \
    SUBDIRS += xcomposite-egl
qtConfig(xcomposite-glx): \
    SUBDIRS += xcomposite-glx
qtConfig(drm-egl-server): \
    SUBDIRS += drm-egl-server
qtConfig(libhybris-egl-server): \
    SUBDIRS += libhybris-egl-server
