import QtQuick 2.0

Rectangle {
    id: root
    width: 500
    height: 600

    property int duration: 10
    property int count: grid.count

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper

            property string nameData: name

            objectName: "wrapper"
            width: 80
            height: 60
            border.width: 1
            Column {
                Text { text: index }
                Text {
                    text: wrapper.x + ", " + wrapper.y
                }
                Text {
                    id: textName
                    objectName: "textName"
                    text: name
                }
            }
            color: GridView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }

    GridView {
        id: grid

        property var displaceTransitionsStarted: new Object()
        property bool displaceTransitionsDone: false

        objectName: "grid"
        focus: true
        anchors.centerIn: parent
        width: 240
        height: 320
        cacheBuffer: 0
        cellWidth: 80
        cellHeight: 60
        model: testModel
        delegate: myDelegate

        displaced: Transition {
            id: transition
            SequentialAnimation {
                ScriptAction {
                    script: {
                        var name = transition.ViewTransition.item.nameData
                        if (grid.displaceTransitionsStarted[name] == undefined)
                            grid.displaceTransitionsStarted[name] = 0
                        grid.displaceTransitionsStarted[name] += 1
                    }
                }
                NumberAnimation {
                    properties: "x,y"
                    duration: root.duration
                    easing.type: Easing.OutBounce
                    easing.amplitude: 10.0      // longer-lasting bounce to trigger bug
                }
                PropertyAction { target: grid; property: "displaceTransitionsDone"; value: true }
            }
        }
    }

    Rectangle {
        anchors.fill: grid
        color: "lightsteelblue"
        opacity: 0.2
    }
}

