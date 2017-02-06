QT += charts

HEADERS += \
    brushtool.h \
    customslice.h \
    mainwidget.h \
    pentool.h

SOURCES += \
    brushtool.cpp \
    customslice.cpp \
    main.cpp \
    mainwidget.cpp \
    pentool.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/charts/piechartcustomization
INSTALLS += target
