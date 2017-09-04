INCLUDEPATH += $$PWD

macos: LIBS += -L$$OUT_PWD/../grue_app.app/Contents/Frameworks
else: LIBS += -L$$OUT_PWD/..

LIBS += -lgruesensor
