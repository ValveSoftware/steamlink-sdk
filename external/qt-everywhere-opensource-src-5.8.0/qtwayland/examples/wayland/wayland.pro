requires(qtHaveModule(waylandcompositor))
requires(qtConfig(opengl))
TEMPLATE=subdirs

SUBDIRS += \
    qwindow-compositor
    minimal-cpp

qtHaveModule(quick) {
    SUBDIRS += minimal-qml
    SUBDIRS += spanning-screens
    SUBDIRS += pure-qml
    SUBDIRS += multi-output
    SUBDIRS += multi-screen
    SUBDIRS += ivi-compositor
    qtHaveModule(waylandclient) {
        SUBDIRS += \
            custom-extension \
            server-buffer
    }
}
