import QtQuick 2.0

Rectangle {
    width: 400;
    height: 240;
    color: "black";

    Rectangle {
        id: gr
        objectName: "target"
        color: "green"
        width: 50; height: 50
    }

    Rectangle {
        id: np
        objectName: "newParent"
        x: 150
        width: 150; height: 150
        color: "yellow"
        clip: true
        Rectangle {
            color: "red"
            x: 50; y: 50; height: 50; width: 50
        }

    }

    Rectangle {
        id: vp
        objectName: "viaParent"
        x: 100; y: 100
        width: 50; height: 50
        color: "blue"
        rotation: 45
        scale: 2
    }

    states: State {
        name: "state1"
        ParentChange {
            target: gr
            parent: np
            x: 50; y: 50; width: 100;
        }
    }

    transitions: Transition {
        reversible: true
        to: "state1"
        ParentAnimation {
            target: gr; via: vp;
            NumberAnimation { properties: "x,y,rotation,scale,width" }
        }
    }
}
