QT+=widgets

HEADERS += desktopwidget.h contenttab.h linktab.h
SOURCES += desktopwidget.cpp contenttab.cpp linktab.cpp main.cpp

RESOURCES += desktopservices.qrc

EXAMPLE_FILES += data

music.files = data/*.mp3 data/*.wav
image.files = data/*.png

target.path = $$[QT_INSTALL_EXAMPLES]/svg/embedded/desktopservices
INSTALLS += target
