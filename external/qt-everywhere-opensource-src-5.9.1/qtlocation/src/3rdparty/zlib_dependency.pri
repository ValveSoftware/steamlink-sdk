# zlib dependency satisfied by bundled 3rd party zlib or system zlib
qtConfig(system-zlib) {
    QMAKE_USE_PRIVATE += zlib
} else {
    QT_PRIVATE += zlib-private
}
