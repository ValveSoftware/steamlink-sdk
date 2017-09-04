import QtQuick 2.0

Rectangle {
    id: focusoutwindow

    width: 100
    height: 150

    property alias text1: text1
    property alias text2: text2

    TextEdit {
        x: 20
        y: 30
        id: text1
        text: "text 1"
        height: 20
        width: 50
        selectByMouse: true
        activeFocusOnPress: true
        objectName: "text1"
    }

    TextEdit {
        x: 20
        y: 80
        id: text2
        text: "text 2"
        height: 20
        width: 50
        selectByMouse: true
        activeFocusOnPress: true
        objectName: "text2"
    }
}
