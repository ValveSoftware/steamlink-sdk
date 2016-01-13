import QtQuick 1.0

Rectangle {
    id: root
    width: 200
    height: 200
    color: "black"

    VisualDataModel {
        id: model
        model: ListModel {
            ListElement { itemColor: "red" }
            ListElement { itemColor: "green" }
            ListElement { itemColor: "blue" }
            ListElement { itemColor: "orange" }
            ListElement { itemColor: "purple" }
            ListElement { itemColor: "yellow" }
            ListElement { itemColor: "slategrey" }
            ListElement { itemColor: "cyan" }
        }
        delegate: Package {
            Rectangle {
                id: listItem; Package.name: "list"; width:root.width/2; height: 25; color: "transparent"; border.color: "white"
                MouseArea {
                    anchors.fill: parent
                    onClicked: myState.state = myState.state == "list" ? "grid" : "list"
                }
            }
            Rectangle {
                id: gridItem; Package.name: "grid"; width:50; height: 50; color: "transparent"; border.color: "white"
                MouseArea {
                    anchors.fill: parent
                    onClicked: myState.state = myState.state == "list" ? "grid" : "list"
                }
            }
            Rectangle { id: myContent; width:50; height: 50; color: itemColor }

            StateGroup {
                id: myState
                state: "list"
                states: [
                    State {
                        name: "list"
                        ParentChange { target: myContent; parent: listItem }
                        PropertyChanges { target: myContent; x: 0; y: 0; width: listItem.width; height: listItem.height }
                    },
                    State {
                        name: "grid"
                        ParentChange { target: myContent; parent: gridItem }
                        PropertyChanges { target: myContent; x: 0; y: 0; width: gridItem.width; height: gridItem.height }
                    }
                ]

                transitions: [
                    Transition {
                        from: "*"; to: "*"
                        SequentialAnimation {
                            ParentAnimation{
                                NumberAnimation { properties: "x,y,width,height"; easing.type: "InOutQuad" }
                            }
                        }
                    }
                ]
            }
        }
    }

    ListView {
        width: parent.width/2
        height: parent.height
        model: model.parts.list
    }

    GridView {
        x: parent.width/2
        width: parent.width/2
        cellWidth: 50
        cellHeight: 50
        height: parent.height
        model: model.parts.grid
    }
}
