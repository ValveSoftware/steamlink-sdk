SOURCES += \
    $$PWD/testrenderer.cpp

HEADERS += \
    $$PWD/testrenderer.h

useCommonTestAspect {
  SOURCES += $$PWD/testaspect.cpp
  SOURCES += $$PWD/testaspect.h
}

INCLUDEPATH += $$PWD

QT += core-private 3dcore 3dcore-private 3drender 3drender-private
