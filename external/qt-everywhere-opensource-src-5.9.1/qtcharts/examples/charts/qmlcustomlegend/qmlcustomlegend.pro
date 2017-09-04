QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlcustomlegend/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlcustomlegend
INSTALLS += target
