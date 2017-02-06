TEMPLATE = app
TARGET = heartlistener

QT += quick bluetooth

# Input
HEADERS += deviceinfo.h \
           heartrate.h
SOURCES += deviceinfo.cpp \
           heartrate.cpp \
           main.cpp

OTHER_FILES += assets/*.qml \
               assets/*.js

RESOURCES += \
             resources.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/bluetooth/heartlistener
INSTALLS += target
