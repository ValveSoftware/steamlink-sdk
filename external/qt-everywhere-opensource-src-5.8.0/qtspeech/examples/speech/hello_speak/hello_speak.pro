TEMPLATE = app

QT += widgets texttospeech

SOURCES = main.cpp \
          mainwindow.cpp
HEADERS = mainwindow.h

FORMS = mainwindow.ui

# install
target.path = $$[QT_INSTALL_EXAMPLES]/speech/hello_speak
INSTALLS += target
