TEMPLATE = app

QT += qml quick widgets xmlpatterns

SOURCES += main.cpp

lupdate_only{
SOURCES = *.qml \
          PhotoViewerCore/*.qml \
          PhotoViewerCore/script/*.js
}

TRANSLATIONS += i18n/qml_fr.ts \
                i18n/qml_de.ts

RESOURCES += qml.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/quick/demos/photoviewer
INSTALLS += target
