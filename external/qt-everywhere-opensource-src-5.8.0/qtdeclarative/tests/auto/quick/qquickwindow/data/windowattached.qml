import QtQuick 2.4
import QtQuick.Window 2.2

Rectangle {
    id: root
    width: 100
    height: 100
    property bool windowActive: root.Window.active
    property Item contentItem: root.Window.contentItem
    property int windowWidth: root.Window.width
    property int windowHeight: root.Window.height
    property var window: root.Window.window
    Text {
        objectName: "rectangleWindowText"
        anchors.centerIn: parent
        text: (windowActive ? "active" : "inactive") + "\nvisibility: " + root.Window.visibility
    }

    property Window extraWindow: Window {
        objectName: "extraWindow"
        title: "extra window"
        visible: true
        Text {
            objectName: "extraWindowText"
            anchors.centerIn: parent
            text: (extraWindow.active ? "active" : "inactive") + "\nvisibility: " + Window.visibility
            property Item contentItem: Window.contentItem
            property int windowWidth: Window.width
            property int windowHeight: Window.height
            property var window: Window.window
        }
    }
}
