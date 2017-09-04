include(lipicommon.pri)

CONFIG -= static
CONFIG += plugin

DESTDIR = $$MODULE_BASE_OUTDIR/plugins/lipi_toolkit

target.path = $$[QT_INSTALL_PLUGINS]/lipi_toolkit
INSTALLS += target
