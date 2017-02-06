QT = core widgets network networkauth
CONFIG -= app_bundle

HEADERS += \
    twitter.h \
    twittertimelinemodel.h

SOURCES += \
    main.cpp \
    twitter.cpp \
    twittertimelinemodel.cpp

FORMS += \
    twitterdialog.ui

# install
target.path = $$[QT_INSTALL_EXAMPLES]/oauth/twittertimeline
INSTALLS += target
