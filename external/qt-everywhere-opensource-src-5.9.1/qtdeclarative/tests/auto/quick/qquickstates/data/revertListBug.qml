import QtQuick 2.0

Rectangle {
    width: 400; height: 400

    property Item targetItem: rect1

    function switchTargetItem() {
        if (targetItem === rect1)
            targetItem = rect2;
        else
            targetItem = rect1;
    }

    states: State {
        name: "reparented"
        ParentChange {
            target: targetItem
            parent: newParent
            x: 0; y: 0
        }
    }

    Item {
        objectName: "originalParent1"
        Rectangle {
            id: rect1; objectName: "rect1"
            width: 50; height: 50
            color: "green"
        }
    }

    Item {
        objectName: "originalParent2"
        Rectangle {
            id: rect2; objectName: "rect2"
            x: 50; y: 50
            width: 50; height: 50
            color: "green"
        }
    }

    Item {
        id: newParent; objectName: "newParent"
        x: 200; y: 100
    }
}
