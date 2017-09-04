QT += charts

HEADERS += \
    mainwidget.h

SOURCES += \
    main.cpp \
    mainwidget.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/legendmarkers
INSTALLS += target
