QMAKE_DOCS = $$PWD/qtquickcontrols2.qdocconf

OTHER_FILES += \
    $$files($$PWD/snippets/*.qml) \
    $$files($$PWD/src/*.qdoc) \
    $$files($$PWD/src/calendar/*.qdoc) \
    $$files($$PWD/src/templates/*.qdoc)
