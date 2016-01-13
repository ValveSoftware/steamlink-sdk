import QtQuick 1.0

Rectangle {
    color: "white"
    width: 800
    height: 600

    Text { text: "All five rectangles should be red" }

    FocusScope {
        y: 100
        focus: true; objectName: "item1"
        Rectangle { width: 50; height: 50; color: parent.activeFocus?"red":"blue" }

        FocusScope {
            y: 100
            focus: true; objectName: "item2"
            Rectangle { width: 50; height: 50; color: parent.activeFocus?"red":"blue" }

            FocusScope {
                y: 100
                focus: true; objectName: "item3"
                Rectangle { width: 50; height: 50; color: parent.activeFocus?"red":"blue" }

                FocusScope {
                    y: 100
                    focus: true; objectName: "item4"
                    Rectangle { width: 50; height: 50; color: parent.activeFocus?"red":"blue" }

                    FocusScope {
                        y: 100
                        focus: true; objectName: "item5"
                        Rectangle { width: 50; height: 50; color: parent.activeFocus?"red":"blue" }
                    }
                }
            }
        }
    }
}
