QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlaxes/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlaxes
INSTALLS += target
