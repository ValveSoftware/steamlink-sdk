import QtQuick 2.7

ListView {
    width: 400
    height: 400
    model: 100
    // Ensure that the property is available in QML.
    onKeyNavigationEnabledChanged: {}
    delegate: Rectangle {
        height: 40
        width: 400
        color: index % 2 ? "lightsteelblue" : "lightgray"
    }
}
