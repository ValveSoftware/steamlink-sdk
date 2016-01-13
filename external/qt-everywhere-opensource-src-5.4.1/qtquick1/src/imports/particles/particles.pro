CXX_MODULE = declarative
TARGET  = qmlparticlesplugin
TARGETPATH = Qt/labs/particles

QT += declarative declarative-private core-private gui-private widgets-private

SOURCES += \
    qdeclarativeparticles.cpp \
    particles.cpp

HEADERS += \
    qdeclarativeparticles_p.h

OTHER_FILES += particles.json

load(qml1_plugin)
