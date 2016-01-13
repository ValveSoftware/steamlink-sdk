TEMPLATE = app
TARGET = musicplayer

QT += widgets multimedia winextras

HEADERS = \
    musicplayer.h \
    volumebutton.h

SOURCES = \
    main.cpp \
    musicplayer.cpp \
    volumebutton.cpp

RESOURCES += \
    musicplayer.qrc

RC_ICONS = images/musicplayer.ico

target.path = $$[QT_INSTALL_EXAMPLES]/winextras/musicplayer
INSTALLS += target
