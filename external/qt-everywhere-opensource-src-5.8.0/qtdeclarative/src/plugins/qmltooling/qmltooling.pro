TEMPLATE = subdirs
QT_FOR_CONFIG += qml

# Utilities
SUBDIRS += \
    packetprotocol

# Connectors
SUBDIRS += \
    qmldbg_native \
    qmldbg_server

qtConfig(qml-network) {
    SUBDIRS += \
        qmldbg_local \
        qmldbg_tcp
}

# Services
SUBDIRS += \
    qmldbg_debugger \
    qmldbg_profiler

qmldbg_server.depends = packetprotocol
qmldbg_native.depends = packetprotocol
qmldbg_debugger.depends = packetprotocol
qmldbg_profiler.depends = packetprotocol

qtHaveModule(quick) {
    SUBDIRS += \
        qmldbg_inspector \
        qmldbg_quickprofiler
    qmldbg_inspector.depends = packetprotocol
    qmldbg_quickprofiler.depends = packetprotocol
}
