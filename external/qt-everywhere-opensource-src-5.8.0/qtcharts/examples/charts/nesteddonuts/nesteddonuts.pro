QT += charts

HEADERS += \
    widget.h

SOURCES += \
    main.cpp \
    widget.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/nesteddonuts
INSTALLS += target
