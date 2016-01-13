import QtQuick 1.0

Rectangle {
    color: "white"
    width: 400
    height: 50

    //All five rectangles should be red
    FocusScope {
        focus: true
        Rectangle { width: 40; height: 40; color: parent.activeFocus?"red":"blue" }

        FocusScope {
            x: 80
            focus: true
            Rectangle { width: 40; height: 40; color: parent.activeFocus?"red":"blue" }

            FocusScope {
                x: 80
                focus: true
                Rectangle { width: 40; height: 40; color: parent.activeFocus?"red":"blue" }

                FocusScope {
                    x: 80
                    focus: true
                    Rectangle { width: 40; height: 40; color: parent.activeFocus?"red":"blue" }

                    FocusScope {
                        x: 80
                        focus: true
                        Rectangle { width: 40; height: 40; color: parent.activeFocus?"red":"blue" }
                    }
                }
            }
        }
    }

}
