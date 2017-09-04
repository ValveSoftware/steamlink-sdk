QT += widgets scxml

CONFIG += c++11

STATECHARTS = sudoku.scxml

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

RESOURCES += \
    sudoku.qrc

# install
target.path = $$[QT_INSTALL_EXAMPLES]/scxml/sudoku
INSTALLS += target
