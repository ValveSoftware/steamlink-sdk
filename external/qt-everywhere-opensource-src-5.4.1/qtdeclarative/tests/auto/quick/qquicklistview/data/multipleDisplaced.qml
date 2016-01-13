import QtQuick 2.0

Rectangle {
    id: root
    width: 500
    height: 600

    property int duration: 10
    property int count: list.count

    Component {
        id: myDelegate
        Rectangle {
            id: wrapper

            property string nameData: name

            objectName: "wrapper"
            height: 20
            width: 240
            Text { text: index }
            Text {
                x: 30
                id: textName
                objectName: "textName"
                text: name
            }
            Text {
                x: 200
                text: wrapper.y
            }
            color: ListView.isCurrentItem ? "lightsteelblue" : "white"
        }
    }

    ListView {
        id: list

        property var displaceTransitionsStarted: new Object()
        property bool displaceTransitionsDone: false

        objectName: "list"
        focus: true
        anchors.centerIn: parent
        width: 240
        height: 320
        cacheBuffer: 60
        model: testModel
        delegate: myDelegate

        displaced: Transition {
            id: transition
            SequentialAnimation {
                ScriptAction {
                    script: {
                        var name = transition.ViewTransition.item.nameData
                        if (list.displaceTransitionsStarted[name] == undefined)
                            list.displaceTransitionsStarted[name] = 0
                        list.displaceTransitionsStarted[name] += 1
                    }
                }
                NumberAnimation {
                    properties: "x,y"
                    duration: root.duration
                    easing.type: Easing.OutBounce
                    easing.amplitude: 10.0      // longer-lasting bounce to trigger bug
                }
                PropertyAction { target: list; property: "displaceTransitionsDone"; value: true }
            }
        }
    }

    Rectangle {
        anchors.fill: list
        color: "lightsteelblue"
        opacity: 0.2
    }
}

