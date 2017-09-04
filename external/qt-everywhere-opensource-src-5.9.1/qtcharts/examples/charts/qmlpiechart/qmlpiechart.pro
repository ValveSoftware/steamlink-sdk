QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlpiechart/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlpiechart
INSTALLS += target
