QT += charts

HEADERS += \
    chart.h \
    chartview.h

SOURCES += \
    main.cpp \
    chart.cpp \
    chartview.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/zoomlinechart
INSTALLS += target
