import QtQuick 1.0

Rectangle {
    color: "white"
    width: 800
    height: 600

    Keys.onDigit9Pressed: console.log("Error - Root")

    FocusScope {
        id: myScope

        Keys.onDigit9Pressed: console.log("Error - FocusScope")

        Rectangle {
            objectName: "item0"
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
                Keys.onDigit9Pressed: console.log("Error - Top Left");
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
                Keys.onDigit9Pressed: console.log("Error - Top Right");

                Rectangle {
                    width: 50; height: 50; anchors.centerIn: parent
                    color: parent.activeFocus?"red":"transparent"
                }
            }
        }
        KeyNavigation.down: item3
    }

    Text { x:100; y:170; text: "There should be no blue borders, or red squares.\nPressing \"9\" should do nothing.\nArrow keys should have no effect." }

    Rectangle {
        id: item3; objectName: "item3"
        x: 10; y: 300
        width: 100; height: 100; color: "green"
        border.width: 5
        border.color: activeFocus?"blue":"black"

        Keys.onDigit9Pressed: console.log("Error - Bottom Left");
        KeyNavigation.up: myScope

        Rectangle {
            width: 50; height: 50; anchors.centerIn: parent
            color: parent.activeFocus?"red":"transparent"
        }
    }

}
