QT += charts

HEADERS += \
    chart.h \
    chartview.h

SOURCES += \
    chart.cpp \
    chartview.cpp \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/chartinteractions
INSTALLS += target
