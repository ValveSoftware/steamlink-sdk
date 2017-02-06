QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlcustomizations/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlcustomizations
INSTALLS += target
