QT += charts

HEADERS += \
    drilldownchart.h \
    drilldownseries.h

SOURCES += \
    drilldownchart.cpp \
    drilldownseries.cpp \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/stackedbarchartdrilldown
INSTALLS += target
