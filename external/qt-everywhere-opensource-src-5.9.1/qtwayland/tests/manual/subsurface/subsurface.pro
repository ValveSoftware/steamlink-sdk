TEMPLATE = app

QT += \
    gui gui-private core-private\
    quick \
    waylandclient \

HEADERS += \
    shmwindow.h \

SOURCES += \
    main.cpp \
    shmwindow.cpp \

RESOURCES += \
    qml.qrc

