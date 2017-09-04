HEADERS       = mainwindow.h \
                svgview.h \
                exportdialog.h
RESOURCES     = svgviewer.qrc
SOURCES       = main.cpp \
                mainwindow.cpp \
                svgview.cpp \
                exportdialog.cpp
QT           += widgets svg

qtHaveModule(opengl): QT += opengl

CONFIG += console

# install
target.path = $$[QT_INSTALL_EXAMPLES]/svg/svgviewer
INSTALLS += target
