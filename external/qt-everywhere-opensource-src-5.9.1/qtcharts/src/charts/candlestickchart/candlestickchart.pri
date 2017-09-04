INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

SOURCES += \
    $$PWD/candlestick.cpp \
    $$PWD/candlestickchartitem.cpp \
    $$PWD/qcandlestickseries.cpp \
    $$PWD/qcandlestickset.cpp \
    $$PWD/qcandlestickmodelmapper.cpp \
    $$PWD/qhcandlestickmodelmapper.cpp \
    $$PWD/qvcandlestickmodelmapper.cpp

PRIVATE_HEADERS += \
    $$PWD/candlestick_p.h \
    $$PWD/candlestickchartitem_p.h \
    $$PWD/candlestickdata_p.h \
    $$PWD/qcandlestickseries_p.h \
    $$PWD/qcandlestickset_p.h \
    $$PWD/qcandlestickmodelmapper_p.h

PUBLIC_HEADERS += \
    $$PWD/qcandlestickseries.h \
    $$PWD/qcandlestickset.h \
    $$PWD/qcandlestickmodelmapper.h \
    $$PWD/qhcandlestickmodelmapper.h \
    $$PWD/qvcandlestickmodelmapper.h
