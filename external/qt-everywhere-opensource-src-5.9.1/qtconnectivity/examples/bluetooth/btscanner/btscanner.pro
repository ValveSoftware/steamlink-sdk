TARGET = btscanner

QT = core bluetooth widgets
TEMPLATE = app

SOURCES = \
    main.cpp \
    device.cpp \
    service.cpp

HEADERS = \
    device.h \
    service.h

FORMS = \
    device.ui \
    service.ui

target.path = $$[QT_INSTALL_EXAMPLES]/bluetooth/btscanner
INSTALLS += target
