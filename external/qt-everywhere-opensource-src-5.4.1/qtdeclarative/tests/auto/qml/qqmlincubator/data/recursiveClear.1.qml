import QtQuick 2.0

Rectangle {
    objectName: "switchMe"
    signal switchMe
    width: 100; height: 100
    color: "green"
    Component.onCompleted: switchMe()
}
