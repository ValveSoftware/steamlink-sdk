import QtQuick 2.4
import QtQuick.Window 2.2

Window {
    id : mainWindow
    visible: true
    width: 800
    height: 480

    property real gridListWidth : (width * 0.60)
    property real gridListHeight : (height * 0.50)

    property real gridCellSpacing : (height * 0.004)
    property real gridCellHeight : (height * 0.039)
    property real gridCellWidth : (width * 0.20)

    Rectangle {
        id : rectBackground
        anchors.fill: parent
        color : "white"

        ListView {
            id : ls
            width: mainWindow.gridListWidth
            height: mainWindow.gridListHeight
            clip : true
            headerPositioning: ListView.OverlayHeader
            spacing : mainWindow.gridCellSpacing

            model: ListModel {
                ListElement {
                    name: "Bill Smith"
                    number: "555 3264"
                    hairColor: "red"
                }
                ListElement {
                    name: "John Brown"
                    number: "484 7789"
                    hairColor: "blue"
                }
                ListElement {
                    name: "Sam Wise"
                    number: "284 1547"
                    hairColor: "yellow"
                }
            }

            header : Row {
                spacing : mainWindow.gridCellSpacing

                Rectangle {
                    width : mainWindow.gridCellWidth
                    height : mainWindow.gridCellHeight
                    color : "blue"

                    Text {
                        anchors.centerIn: parent
                        color : "white"
                        text: "Name"
                    }
                }

                Rectangle {
                    width : mainWindow.gridCellWidth
                    height : mainWindow.gridCellHeight
                    color : "blue"

                    Text {
                        anchors.centerIn: parent
                        color : "white"
                        text: "Number"
                    }

                }

                Rectangle {
                    width : mainWindow.gridCellWidth
                    height : mainWindow.gridCellHeight
                    color : "blue"

                    Text {
                        anchors.centerIn: parent
                        color : "white"
                        text: "Hair Color"
                    }
                }
            }

            delegate: Row {
                spacing : mainWindow.gridCellSpacing

                Rectangle {
                    width : mainWindow.gridCellWidth
                    height : mainWindow.gridCellHeight
                    color : "red"

                    Text {
                        anchors.centerIn: parent
                        color : "white"
                        text: name
                    }
                }

                Rectangle {
                    width : mainWindow.gridCellWidth
                    height : mainWindow.gridCellHeight
                    color : "red"

                    Text {
                        anchors.centerIn: parent
                        color : "white"
                        text: number
                    }
                }

                Rectangle {
                    width : mainWindow.gridCellWidth
                    height : mainWindow.gridCellHeight
                    color : "red"

                    Text {
                        anchors.centerIn: parent
                        color : "white"
                        text: hairColor
                    }
                }
            }
        }
    }
}
