INCLUDEPATH += ../../../include

LIBS += -L$$OUT_PWD/../../lib

TEMPLATE = app

QT += datavisualization

contains(TARGET, qml.*) {
    QT += qml quick
}

target.path = $$[QT_INSTALL_TESTS]/datavisualization/$$TARGET
INSTALLS += target
