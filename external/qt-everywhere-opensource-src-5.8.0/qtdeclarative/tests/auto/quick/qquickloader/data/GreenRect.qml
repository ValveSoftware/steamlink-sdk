import QtQuick 2.0

Rectangle {
    width: 100; height: 100
    color: "green"
    Component.onCompleted: myLoader.source = "BlueRect.qml"
}
