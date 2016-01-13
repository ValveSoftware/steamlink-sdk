HEADERS       = settingswidget.h \
                plotwidget.h
SOURCES       = settingswidget.cpp \
                plotwidget.cpp \
                main.cpp

QT += webkitwidgets widgets

# install
target.path = $$[QT_INSTALL_EXAMPLES]/webkitwidgets/scroller/plot
INSTALLS += target
