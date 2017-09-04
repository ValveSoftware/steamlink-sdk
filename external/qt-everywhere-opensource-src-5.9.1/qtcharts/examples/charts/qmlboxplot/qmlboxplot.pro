QT += charts qml quick

SOURCES += \
    main.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmlboxplot/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlboxplot
INSTALLS += target
