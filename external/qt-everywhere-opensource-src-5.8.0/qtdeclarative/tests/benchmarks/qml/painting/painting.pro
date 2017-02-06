requires(qtHaveModule(opengl))

QT += opengl
CONFIG += console
macx:CONFIG -= app_bundle

SOURCES += paintbenchmark.cpp
