TEMPLATE = app
QT += widgets quick

SOURCES += main.cpp

OTHER_FILES += main.qml TextBox.qml

RESOURCES += \
    embeddedinwidgets.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/embeddedinwidgets
INSTALLS += target
