import QtQuick 2.0

Item {
    width: 640
    height: 480
    Row {
        Repeater{ model: 3;
            delegate: Component {
                Rectangle {
                    color: "red"
                    width: 50
                    height: 50
                    z: {if(index == 0){2;}else if(index == 1){1;} else{3;}}
                    objectName: {if(index == 0){"one";}else if(index == 1){"two";} else{"three";}}
                }
            }
        }
    }

    //This crashed once (QTBUG-16959) because the repeater ended up on the end of the list
    //If this grid just instantiates without crashing, then it has not regressed.
    Grid {
        id: grid
        rows: 2
        flow: Grid.TopToBottom

        Repeater {
            model: 13
            Rectangle {
                color: "goldenrod"
                width: 100
                height: 100
                radius: 10
                border.width: 1
            }
        }
    }
}
