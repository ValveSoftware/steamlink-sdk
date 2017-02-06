QT += charts

HEADERS += \
    boxdatareader.h

SOURCES += \
    boxdatareader.cpp \
    main.cpp \

RESOURCES += \
    boxplotdata.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/charts/boxplotchart
INSTALLS += target
