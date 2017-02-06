QT += quick qml
!android: !ios: !blackberry: qtHaveModule(widgets): QT += widgets

SOURCES += main.cpp \
    hangmangame.cpp

HEADERS += \
    hangmangame.h

RESOURCES += \
    resources.qrc

QT += purchasing

target.path = $$[QT_INSTALL_EXAMPLES]/purchasing/qthangman
INSTALLS += target

OTHER_FILES += \
    qml/qthangman/GameView.qml \
    qml/qthangman/GuessWordView.qml \
    qml/qthangman/Hangman.qml \
    qml/qthangman/HowToView.qml \
    qml/qthangman/Key.qml \
    qml/qthangman/Letter.qml \
    qml/qthangman/LetterSelector.qml \
    qml/qthangman/main.qml \
    qml/qthangman/MainView.qml \
    qml/qthangman/PageHeader.qml \
    qml/qthangman/ScoreItem.qml \
    qml/qthangman/SimpleButton.qml \
    qml/qthangman/SplashScreen.qml \
    qml/qthangman/StoreItem.qml \
    qml/qthangman/StoreView.qml \
    qml/qthangman/Word.qml \
    enable2.txt

mac {
    #Change the following lines to match your unique App ID
    #to enable in-app purchases on OS X and iOS
    #For example if your App ID is org.qtproject.qt.iosteam.qthangman"
    #QMAKE_TARGET_BUNDLE_PREFIX = "org.qtproject.qt.iosteam"
    #TARGET = qthangman
}

winrt {
    # Add a StoreSimulation.xml file to the root resource folder
    # and Qt Purchasing will switch into simulation mode.
    # Remove the file in the publishing process.
    RESOURCES += \
        winrt/winrt.qrc
}
