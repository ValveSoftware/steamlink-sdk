import QtQuick 2.7

GridView {
    width: 405
    height: 200
    cellWidth: width / 9
    cellHeight: height / 2
    // Ensure that the property is available in QML.
    onKeyNavigationEnabledChanged: {}
    model: 18
    delegate: Rectangle { objectName: "delegate"; width: 10; height: 10; color: "green" }
}
