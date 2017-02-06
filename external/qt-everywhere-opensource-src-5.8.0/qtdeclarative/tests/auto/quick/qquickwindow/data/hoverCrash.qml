import QtQuick 2.0
import QtQuick.Window 2.0 as Window

Window.Window {
    width: 200
    height: 200
    color: "#00FF00"
    Column {
        Rectangle {
            objectName: 'item1'
            color: 'red'
            width: 100
            height: 100
            MouseArea {
                id: area
                anchors.fill: parent
                hoverEnabled: true
            }
        }

        Loader {
            objectName: 'item2'
            width: 100
            height: 100
            active: area.containsMouse
            sourceComponent: Rectangle {
                color: 'blue'

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }
}
