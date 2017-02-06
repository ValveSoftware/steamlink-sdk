QT += charts qml quick

HEADERS += \
    datasource.h

SOURCES += \
    main.cpp \
    datasource.cpp

RESOURCES += \
    resources.qrc

DISTFILES += \
    qml/qmloscilloscope/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmloscilloscope
INSTALLS += target
