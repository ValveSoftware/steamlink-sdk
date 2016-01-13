option(host_build)
TARGET = tst_compile
force_bootstrap {
    QT = bootstrap-private
    !build_pass: CONFIG += release
} else {
    QT = core
    !build_pass:contains(QT_CONFIG, build_all): CONFIG += release
}
QT += qmldevtools-private
macx:CONFIG -= app_bundle

SOURCES += tst_compile.cpp

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
