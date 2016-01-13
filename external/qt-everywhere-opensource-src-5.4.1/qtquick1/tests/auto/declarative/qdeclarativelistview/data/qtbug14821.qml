import QtQuick 1.0

ListView {
    id: view
    width: 300; height: 200
    focus: true
    keyNavigationWraps: true

    model: 100

    preferredHighlightBegin: 90
    preferredHighlightEnd:   110

    highlightRangeMode: ListView.StrictlyEnforceRange
    highlight: Component {
        Rectangle {
            border.color: "blue"
            border.width: 3
            color: "transparent"
            width: 300; height: 15
        }
    }

    delegate: Component {
           Item {
               height: 15 + (view.currentIndex == index ? 20 : 0)
               width: 200
               Text { text: 'Index: ' + index; anchors.verticalCenter: parent.verticalCenter }
           }
       }
}
