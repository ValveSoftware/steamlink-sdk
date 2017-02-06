QT += charts

HEADERS += \
    donutbreakdownchart.h \
    mainslice.h

SOURCES += \
    donutbreakdownchart.cpp \
    main.cpp \
    mainslice.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/donutbreakdown
INSTALLS += target
