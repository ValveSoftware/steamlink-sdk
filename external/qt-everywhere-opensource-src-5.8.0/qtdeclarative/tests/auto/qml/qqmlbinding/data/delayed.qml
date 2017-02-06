import QtQuick 2.8

Item {
    width: 400
    height: 400

    property int changeCount: 0

    property string text1
    property string text2

    function updateText() {
        text1 = "Hello"
        text2 = "World"
    }

    Text {
        anchors.centerIn: parent
        Binding on text {
            value: text1 + " " + text2
            delayed: true
        }
        onTextChanged: ++changeCount
    }
}

