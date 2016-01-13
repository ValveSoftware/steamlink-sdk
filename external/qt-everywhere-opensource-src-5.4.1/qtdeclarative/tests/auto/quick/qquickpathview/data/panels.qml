import QtQuick 2.0

Item {
    id: root
    property bool snapOne: false
    property bool enforceRange: false
    width: 320; height: 480

    VisualItemModel {
        id: itemModel

        Rectangle {
            width: root.width
            height: root.height
            color: "blue"
        }
        Rectangle {
            width: root.width
            height: root.height
            color: "yellow"
        }
        Rectangle {
            width: root.width
            height: root.height
            color: "green"
        }
    }

    PathView {
        id: view
        objectName: "view"
        anchors.fill: parent
        model: itemModel
        preferredHighlightBegin: 0.5
        preferredHighlightEnd: 0.5
        flickDeceleration: 30
        highlightRangeMode: enforceRange ? PathView.StrictlyEnforceRange : PathView.NoHighlightRange
        snapMode: root.snapOne ? PathView.SnapOneItem : PathView.SnapToItem
        path:  Path {
            startX: -root.width; startY: root.height/2
            PathLine { x: root.width*2; y: root.height/2 }
        }
    }
}
