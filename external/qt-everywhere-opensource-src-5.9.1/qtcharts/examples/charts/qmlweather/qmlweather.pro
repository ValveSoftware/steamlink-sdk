QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlweather/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlweather
INSTALLS += target
