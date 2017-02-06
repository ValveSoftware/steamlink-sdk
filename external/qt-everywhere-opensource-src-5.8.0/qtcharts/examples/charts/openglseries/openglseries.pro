QT += charts

HEADERS += \
    datasource.h

SOURCES += \
    datasource.cpp \
    main.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/openglseries
INSTALLS += target
