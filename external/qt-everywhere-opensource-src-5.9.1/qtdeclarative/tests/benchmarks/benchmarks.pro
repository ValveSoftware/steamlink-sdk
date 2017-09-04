TEMPLATE = subdirs
SUBDIRS = qml
qtConfig(private_tests) {
    qtConfig(opengl(es1|es2)?):SUBDIRS += particles
}
