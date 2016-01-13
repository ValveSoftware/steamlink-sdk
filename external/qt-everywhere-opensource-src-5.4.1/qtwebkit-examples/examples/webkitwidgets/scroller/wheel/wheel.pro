HEADERS       = wheelwidget.h
SOURCES       = wheelwidget.cpp \
                main.cpp

QT += webkitwidgets widgets

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webkitwidgets/scroller/wheel
INSTALLS += target
