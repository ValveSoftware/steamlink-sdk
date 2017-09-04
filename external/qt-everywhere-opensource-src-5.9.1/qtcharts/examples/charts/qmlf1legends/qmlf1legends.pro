QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlf1legends/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlf1legends
INSTALLS += target
