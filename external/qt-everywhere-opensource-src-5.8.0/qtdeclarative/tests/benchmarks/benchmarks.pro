TEMPLATE = subdirs
SUBDIRS = qml script
qtConfig(private_tests) {
    qtConfig(opengl(es1|es2)?):SUBDIRS += particles
}
