#DEFINES += QT3D_RENDER_ASPECT_OPENGL_DEBUG

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/graphicscontext_p.h \
    $$PWD/graphicshelperinterface_p.h \
    $$PWD/graphicshelperes2_p.h \
    $$PWD/graphicshelperes3_p.h \
    $$PWD/graphicshelpergl2_p.h \
    $$PWD/graphicshelpergl3_3_p.h \
    $$PWD/graphicshelpergl4_p.h \
    $$PWD/graphicshelpergl3_2_p.h

SOURCES += \
    $$PWD/graphicscontext.cpp \
    $$PWD/graphicshelperes2.cpp \
    $$PWD/graphicshelperes3.cpp \
    $$PWD/graphicshelpergl2.cpp \
    $$PWD/graphicshelpergl3_3.cpp \
    $$PWD/graphicshelpergl4.cpp \
    $$PWD/graphicshelpergl3_2.cpp
