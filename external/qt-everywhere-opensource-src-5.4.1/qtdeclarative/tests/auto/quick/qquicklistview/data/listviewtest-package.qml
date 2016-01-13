import QtQuick 2.0

Rectangle {
    id: root
    width: 240
    height: 320
    color: "#ffffff"

    property int count: list.count
    property bool showHeader: false
    property bool showFooter: false
    property real hr: list.visibleArea.heightRatio
    function heightRatio() {
        return list.visibleArea.heightRatio
    }

    function checkProperties() {
        testObject.error = false;
        if (visualModel.model != testModel) {
            console.log("model property incorrect");
            testObject.error = true;
        }
        if (!testObject.animate && visualModel.delegate != myDelegate) {
            console.log("delegate property incorrect - expected myDelegate");
            testObject.error = true;
        }
        if (testObject.animate && visualModel.delegate != animatedDelegate) {
            console.log("delegate property incorrect - expected animatedDelegate");
            testObject.error = true;
        }
        if (testObject.invalidHighlight && list.highlight != invalidHl) {
            console.log("highlight property incorrect - expected invalidHl");
            testObject.error = true;
        }
        if (!testObject.invalidHighlight && list.highlight != myHighlight) {
            console.log("highlight property incorrect - expected myHighlight");
            testObject.error = true;
        }
    }
    resources: [
        Component {
            id: myDelegate
            Package {
                Rectangle {
                    id: wrapper
                    objectName: "wrapper"
                    height: 20
                    width: 240
                    Package.name: "package"
                    Text {
                        text: index
                    }
                    Text {
                        x: 30
                        id: textName
                        objectName: "textName"
                        text: name
                    }
                    Text {
                        x: 120
                        id: textNumber
                        objectName: "textNumber"
                        text: number
                    }
                    Text {
                        x: 200
                        text: wrapper.y
                    }
                    color: ListView.isCurrentItem ? "lightsteelblue" : "white"
                }
            }
        },
        Component {
            id: animatedDelegate
            Package {
                Rectangle {
                    id: wrapper
                    objectName: "wrapper"
                    height: 20
                    width: 240
                    Package.name: "package"
                    Text {
                        text: index
                    }
                    Text {
                        x: 30
                        id: textName
                        objectName: "textName"
                        text: name
                    }
                    Text {
                        x: 120
                        id: textNumber
                        objectName: "textNumber"
                        text: number
                    }
                    Text {
                        x: 200
                        text: wrapper.y
                    }
                    color: ListView.isCurrentItem ? "lightsteelblue" : "white"
                    ListView.onRemove: SequentialAnimation {
                        PropertyAction { target: wrapper; property: "ListView.delayRemove"; value: true }
                        NumberAnimation { target: wrapper; property: "scale"; to: 0; duration: 250; easing.type: "InOutQuad" }
                        PropertyAction { target: wrapper; property: "ListView.delayRemove"; value: false }

                    }
                }
            }
        },
        Component {
            id: myHighlight
            Rectangle { color: "green" }
        },
        Component {
            id: invalidHl
            SmoothedAnimation {}
        },
        Component {
            id: headerFooter
            Rectangle { height: 30; width: 240; color: "blue" }
        },
        VisualDataModel {
           id: visualModel

           model: testModel
           delegate: testObject.animate ? animatedDelegate : myDelegate
        }

    ]
    ListView {
        id: list
        objectName: "list"
        focus: true
        width: 240
        height: 320
        model: visualModel.parts.package
        highlight: testObject.invalidHighlight ? invalidHl : myHighlight
        highlightMoveVelocity: 100000
        highlightResizeVelocity: 1000
        cacheBuffer: testObject.cacheBuffer
        header: root.showHeader ? headerFooter : null
        footer: root.showFooter ? headerFooter : null
    }
}
