import QtQuick 1.0

Rectangle {
    color: "white"
    width: 800
    height: 600

    Keys.onDigit9Pressed: console.log("Error - Root")

    FocusScope {
        id: myScope
        objectName: "item0"
        focus: true

        Keys.onDigit9Pressed: console.log("Error - FocusScope")

        Rectangle {
            height: 120
            width: 420

            color: "transparent"
            border.width: 5
            border.color: myScope.activeFocus?"blue":"black"

            Rectangle {
                id: item1; objectName: "item1"
                x: 10; y: 10
                width: 100; height: 100; color: "green"
                border.width: 5
                border.color: activeFocus?"blue":"black"
                Keys.onDigit9Pressed: console.debug("Top Left");
                KeyNavigation.right: item2
                focus: true

                Rectangle {
                    width: 50; height: 50; anchors.centerIn: parent
                    color: parent.activeFocus?"red":"transparent"
                }
            }

            Rectangle {
                id: item2; objectName: "item2"
                x: 310; y: 10
                width: 100; height: 100; color: "green"
                border.width: 5
                border.color: activeFocus?"blue":"black"
                KeyNavigation.left: item1
                Keys.onDigit9Pressed: console.log("Top Right");

                Rectangle {
                    width: 50; height: 50; anchors.centerIn: parent
                    color: parent.activeFocus?"red":"transparent"
                }
            }
        }
        KeyNavigation.down: item3
    }

    Text { x:100; y:170; text: "Blue border indicates scoped focus\nBlack border indicates NOT scoped focus\nRed box indicates active focus\nUse arrow keys to navigate\nPress \"9\" to print currently focused item" }

    Rectangle {
        id: item3; objectName: "item3"
        x: 10; y: 300
        width: 100; height: 100; color: "green"
        border.width: 5
        border.color: activeFocus?"blue":"black"

        Keys.onDigit9Pressed: console.log("Bottom Left");
        KeyNavigation.up: myScope

        Rectangle {
            width: 50; height: 50; anchors.centerIn: parent
            color: parent.activeFocus?"red":"transparent"
        }
    }

}
