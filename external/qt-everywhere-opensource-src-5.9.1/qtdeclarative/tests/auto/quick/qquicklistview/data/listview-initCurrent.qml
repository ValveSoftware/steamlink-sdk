import QtQuick 2.0

Rectangle {
    id: root

    property int current: list.currentIndex
    property bool showHeader: false
    property bool showFooter: false
    property int currentItemChangedCount: 0

    width: 240
    height: 320
    color: "#ffffff"
    resources: [
        Component {
            id: myDelegate
            Rectangle {
                id: wrapper
                objectName: "wrapper"
                height: 20
                width: 240
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
    ]

    Component {
        id: headerFooter
        Rectangle { height: 30; width: 240; color: "blue" }
    }

    ListView {
        id: list
        objectName: "list"
        focus: true
        currentIndex: 20
        width: 240
        height: 320
        keyNavigationWraps: testWrap
        delegate: myDelegate
        highlightMoveVelocity: 1000
        model: testModel
        header: root.showHeader ? headerFooter : null
        footer: root.showFooter ? headerFooter : null

        onCurrentItemChanged: { root.currentItemChangedCount++ }
    }
}
