QT += charts qml quick

RESOURCES += resources.qrc

SOURCES += main.cpp

OTHER_FILES += \
    qml/qmlcandlestick/main.qml

DISTFILES += \
    qml/qmlcandlestick/*

target.path = $$[QT_INSTALL_EXAMPLES]/charts/qmlcandlestick
INSTALLS += target
