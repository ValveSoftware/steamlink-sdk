QT += charts

HEADERS += \
    drilldownchart.h \
    drilldownslice.h

SOURCES += \
    drilldownchart.cpp \
    drilldownslice.cpp \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/piechartdrilldown
INSTALLS += target
