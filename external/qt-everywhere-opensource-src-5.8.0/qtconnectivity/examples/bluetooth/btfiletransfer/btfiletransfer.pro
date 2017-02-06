TEMPLATE = app
TARGET = btfiletransfer

QT = core bluetooth widgets

SOURCES = \
    main.cpp \
    remoteselector.cpp \
    progress.cpp \
    pindisplay.cpp

HEADERS = \
    remoteselector.h \
    progress.h \
    pindisplay.h

FORMS = \
    remoteselector.ui \
    progress.ui \
    pindisplay.ui

OTHER_FILES += \
    busy.gif \
    pairing.gif

RESOURCES += \
    btfiletransfer.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/bluetooth/btfiletransfer
INSTALLS += target
