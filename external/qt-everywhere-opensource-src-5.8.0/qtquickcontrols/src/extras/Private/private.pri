QT += gui quick

HEADERS += \
    $$PWD/qquickcircularprogressbar_p.h \
    $$PWD/qquickflatprogressbar_p.h \
    $$PWD/qquickmousethief_p.h \
    $$PWD/qquickmathutils_p.h

SOURCES += \
    $$PWD/qquickcircularprogressbar.cpp \
    $$PWD/qquickflatprogressbar.cpp \
    $$PWD/qquickmousethief.cpp \
    $$PWD/qquickmathutils.cpp

QML_FILES += \
    $$PWD/qmldir \
    $$PWD/CircularButton.qml \
    $$PWD/CircularButtonStyleHelper.qml \
    $$PWD/CircularTickmarkLabel.qml \
    $$PWD/Handle.qml \
    $$PWD/PieMenuIcon.qml \
    $$PWD/TextSingleton.qml
