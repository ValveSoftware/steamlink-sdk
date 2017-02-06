QT += charts

TARGET = candlestickchart

SOURCES += main.cpp \
    candlestickdatareader.cpp

HEADERS += \
    candlestickdatareader.h

RESOURCES += \
    candlestickdata.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/charts/candlestickchart
INSTALLS += target
