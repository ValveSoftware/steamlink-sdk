import QtQuick 1.0

/*
    Blue border indicates scoped focus
    Black border indicates NOT scoped focus
    Red box indicates active focus
    Use arrow keys to navigate
    Press "9" to print currently focused item
*/
Rectangle {
    color: "white"
    width: 480
    height: 480

    Keys.onDigit9Pressed: console.log("Error - Root")

    FocusScope {
        id: myScope
        focus: true

        Keys.onDigit9Pressed: console.log("Error - FocusScope")

        Rectangle {
            height: 120
            width: 420

            color: "transparent"
            border.width: 5
            border.color: myScope.activeFocus?"blue":"black"

            Rectangle {
                id: item1
                x: 10; y: 10
                width: 100; height: 100; color: "green"
                border.width: 5
                border.color: activeFocus?"blue":"black"
                Keys.onDigit9Pressed: console.log("Top Left");
                KeyNavigation.right: item2
                focus: true

                Rectangle {
                    width: 50; height: 50; anchors.centerIn: parent
                    color: parent.focus?"red":"transparent"
                }
            }

            Rectangle {
                id: item2
                x: 310; y: 10
                width: 100; height: 100; color: "green"
                border.width: 5
                border.color: activeFocus?"blue":"black"
                KeyNavigation.left: item1
                Keys.onDigit9Pressed: console.log("Top Right");

                Rectangle {
                    width: 50; height: 50; anchors.centerIn: parent
                    color: parent.focus?"red":"transparent"
                }
            }
        }
        KeyNavigation.down: item3
    }

    Rectangle {
        id: item3
        x: 10; y: 300
        width: 100; height: 100; color: "green"
        border.width: 5
        border.color: activeFocus?"blue":"black"

        Keys.onDigit9Pressed: console.log("Bottom Left");
        KeyNavigation.up: myScope

        Rectangle {
            width: 50; height: 50; anchors.centerIn: parent
            color: parent.focus?"red":"transparent"
        }
    }

}
