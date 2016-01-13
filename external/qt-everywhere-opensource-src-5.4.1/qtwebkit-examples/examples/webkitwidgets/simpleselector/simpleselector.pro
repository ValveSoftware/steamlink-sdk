QT       += webkitwidgets network widgets
FORMS     = window.ui
HEADERS   = window.h
SOURCES   = main.cpp \
            window.cpp

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webkitwidgets/simpleselector
INSTALLS += target
