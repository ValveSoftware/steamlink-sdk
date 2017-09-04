import QtQuick 2.1

Item {
    id: main
    objectName: "main"
    width: 400
    height: 700
    focus: true
    Component.onCompleted: button1.focus = true
    Item {
        id: sub1
        objectName: "sub1"
        activeFocusOnTab: false
        width: 100
        height: 50
        anchors.top: parent.top
        Item {
            id: button1
            objectName: "button1"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
    Row {
        id: row_repeater
        objectName: "row_repeater"
        activeFocusOnTab: false
        anchors.top: sub1.bottom
        Repeater {
            activeFocusOnTab: false
            model: 3
            Rectangle {
                activeFocusOnTab: true
                width: 100; height: 40
                border.width: 1
                color: activeFocus ? "red" : "yellow"
            }
        }
    }
    Item {
        id: sub2
        objectName: "sub2"
        activeFocusOnTab: false
        anchors.top: row_repeater.bottom
        width: 100
        height: 50
        Item {
            id: button2
            objectName: "button2"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
    Row {
        id: row
        objectName: "row"
        activeFocusOnTab: false
        anchors.top: sub2.bottom
        Rectangle { activeFocusOnTab: true; color: activeFocus ? "red" : "yellow"; width: 50; height: 50 }
        Rectangle { activeFocusOnTab: true; color: activeFocus ? "red" : "green"; width: 20; height: 50 }
        Rectangle { activeFocusOnTab: true; color: activeFocus ? "red" : "blue"; width: 50; height: 20 }
    }
    Item {
        id: sub3
        objectName: "sub3"
        activeFocusOnTab: false
        anchors.top: row.bottom
        width: 100
        height: 50
        Item {
            id: button3
            objectName: "button3"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
    Flow {
        id: flow
        objectName: "flow"
        activeFocusOnTab: false
        anchors.top: sub3.bottom
        width: parent.width
        anchors.margins: 4
        spacing: 10
        Rectangle { activeFocusOnTab: true; color: activeFocus ? "red" : "yellow"; width: 50; height: 50 }
        Rectangle { activeFocusOnTab: true; color: activeFocus ? "red" : "green"; width: 20; height: 50 }
        Rectangle { activeFocusOnTab: true; color: activeFocus ? "red" : "blue"; width: 50; height: 20 }
    }
    Item {
        id: sub4
        objectName: "sub4"
        activeFocusOnTab: false
        anchors.top: flow.bottom
        width: 100
        height: 50
        Item {
            id: button4
            objectName: "button4"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
    FocusScope {
        id: focusscope
        objectName: "focusscope"
        activeFocusOnTab: false
        anchors.top: sub4.bottom
        height: 40
        Row {
            id: row_focusscope
            objectName: "row_focusscope"
            activeFocusOnTab: false
            anchors.fill: parent
            Repeater {
                activeFocusOnTab: false
                model: 3
                Rectangle {
                    activeFocusOnTab: true
                    width: 100; height: 40
                    border.width: 1
                    color: activeFocus ? "red" : "yellow"
                }
            }
        }
    }
    Item {
        id: sub5
        objectName: "sub5"
        activeFocusOnTab: false
        anchors.top: focusscope.bottom
        width: 100
        height: 50
        Item {
            id: button5
            objectName: "button5"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
    FocusScope {
        id: focusscope2
        objectName: "focusscope2"
        activeFocusOnTab: true
        anchors.top: sub5.bottom
        height: 40
        Row {
            id: row_focusscope2
            objectName: "row_focusscope2"
            activeFocusOnTab: false
            anchors.fill: parent
            Repeater {
                activeFocusOnTab: false
                model: 3
                Rectangle {
                    activeFocusOnTab: true
                    focus: true
                    width: 100; height: 40
                    border.width: 1
                    color: activeFocus ? "red" : "yellow"
                }
            }
        }
    }
    Item {
        id: sub6
        objectName: "sub6"
        activeFocusOnTab: false
        anchors.top: focusscope2.bottom
        width: 100
        height: 50
        Item {
            id: button6
            objectName: "button6"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
    FocusScope {
        id: focusscope3
        objectName: "focusscope3"
        activeFocusOnTab: true
        anchors.top: sub6.bottom
        height: 40
        Row {
            id: row_focusscope3
            objectName: "row_focusscope3"
            activeFocusOnTab: false
            anchors.fill: parent
            Repeater {
                activeFocusOnTab: false
                model: 3
                Rectangle {
                    activeFocusOnTab: true
                    width: 100; height: 40
                    border.width: 1
                    color: activeFocus ? "red" : "yellow"
                }
            }
        }
    }
    Item {
        id: sub7
        objectName: "sub7"
        activeFocusOnTab: false
        anchors.top: focusscope3.bottom
        width: 100
        height: 50
        Item {
            id: button7
            objectName: "button7"
            width: 100
            height: 50
            activeFocusOnTab: true
            Rectangle {
                anchors.fill: parent
                color: parent.activeFocus ? "red" : "black"
            }
        }
    }
}
