QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlpolarchart/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlpolarchart
INSTALLS += target
