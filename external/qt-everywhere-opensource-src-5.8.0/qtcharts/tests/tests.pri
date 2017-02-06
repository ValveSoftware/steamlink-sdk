TEMPLATE = app

QT += charts

contains(TARGET, qml.*) {
    QT += qml quick
}

target.path = $$[QT_INSTALL_TESTS]/charts/$$TARGET
INSTALLS += target
