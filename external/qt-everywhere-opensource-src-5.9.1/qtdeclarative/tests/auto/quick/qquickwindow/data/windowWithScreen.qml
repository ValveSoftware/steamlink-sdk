import QtQuick 2.0
import QtQuick.Window 2.3 as Window

Window.Window {
    color: "#00FF00"
    screen: Qt.application.screens[0]
    Item {
        objectName: "item"
    }
}
