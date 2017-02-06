TEMPLATE=subdirs
QT_FOR_CONFIG += waylandclient-private

qtConfig(wayland-client): \
    SUBDIRS += client cmake
qtHaveModule(waylandcompositor): \
    SUBDIRS += compositor
