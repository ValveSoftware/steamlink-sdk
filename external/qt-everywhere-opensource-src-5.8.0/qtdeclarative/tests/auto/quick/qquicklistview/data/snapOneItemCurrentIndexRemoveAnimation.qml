import QtQuick 2.4

ListView {
    id: root
    height: 400
    width: height
    model: ListModel {
        id: lmodel
        ListElement { dummy: 0 }
        ListElement { dummy: 0 }
        ListElement { dummy: 0 }
        ListElement { dummy: 0 }
        ListElement { dummy: 0 }
        ListElement { dummy: 0 }
    }

    function removeItemZero()
    {
        lmodel.remove(0);
    }

    orientation: ListView.Horizontal
    snapMode: ListView.SnapOneItem
    highlightRangeMode: ListView.StrictlyEnforceRange

    property int transitionsRun: 0

    removeDisplaced: Transition {
        id: transition
        PropertyAnimation { property: "x"; duration: 500 }
        onRunningChanged: if (!running) transitionsRun++;
    }

    delegate: Text {
        text: index + " of " + lmodel.count
        width: root.width
        height: root.height
    }
}