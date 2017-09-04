TARGET = wayland_scanner

isEmpty(QMAKE_WAYLAND_SCANNER):error("QMAKE_WAYLAND_SCANNER not defined for this mkspec")

# Input
SOURCES += main.cpp

wayland-check-header.name = wayland ${QMAKE_FILE_BASE}
wayland-check-header.input = WAYLANDCHECKSOURCES
wayland-check-header.variable_out = HEADERS
wayland-check-header.output = wayland-${QMAKE_FILE_BASE}-client-protocol$${first(QMAKE_EXT_H)}
wayland-check-header.commands = $$QMAKE_WAYLAND_SCANNER client-header < ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}
silent:wayland-client-header.commands = @echo Wayland scanner check header ${QMAKE_FILE_IN} && $$wayland-check-header.commands
QMAKE_EXTRA_COMPILERS += wayland-check-header

WAYLANDCHECKSOURCES = scanner-test.xml
