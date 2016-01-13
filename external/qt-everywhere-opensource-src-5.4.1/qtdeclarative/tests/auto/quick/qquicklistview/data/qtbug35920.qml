import QtQuick 2.1

ListView {
    width: 200
    height: 200

    snapMode: ListView.SnapOneItem
    highlightRangeMode: ListView.StrictlyEnforceRange

    model: 100
    delegate: Text { text: modelData }
}
