HEADERS       = mainwindow.h \
                findfiledialog.h \
                assistant.h \
                textedit.h
SOURCES       = main.cpp \
                mainwindow.cpp \
                findfiledialog.cpp \
                assistant.cpp \
                textedit.cpp

QT += widgets

target.path = $$[QT_INSTALL_EXAMPLES]/assistant/simpletextviewer
docs.files += $$PWD/documentation
docs.path = $$target.path

INSTALLS += target docs
