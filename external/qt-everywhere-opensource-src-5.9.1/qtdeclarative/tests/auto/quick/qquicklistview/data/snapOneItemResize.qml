import QtQuick 2.0

ListView {
    id: list
    currentIndex: 5
    snapMode: ListView.SnapOneItem
    orientation: ListView.Horizontal
    highlightRangeMode: ListView.StrictlyEnforceRange
    highlightFollowsCurrentItem: true
    model: 10
    spacing: 10
    delegate: Item {
        width: ListView.view.width
        height: ListView.view.height
    }
}
