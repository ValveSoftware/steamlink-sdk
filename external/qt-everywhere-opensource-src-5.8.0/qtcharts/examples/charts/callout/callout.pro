QT += charts

HEADERS += \
    callout.h \
    view.h

SOURCES += \
    callout.cpp \
    main.cpp\
    view.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/callout
INSTALLS += target
