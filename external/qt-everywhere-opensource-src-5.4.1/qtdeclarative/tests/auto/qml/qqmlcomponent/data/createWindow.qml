import QtQuick 2.1
import QtQuick.Window 2.1

Window {
    id: window1;
    objectName: "window1";
    color: "#00FF00";
    width: 100; height: 100;
    Item {
        objectName: "item1"
        width: 100; height: 100;
        MouseArea {
            objectName: "mousearea"
            anchors.fill: parent;
            onPressed: window2.requestActivate();
        }
        Component.onCompleted: window2.show();
    }

    Window {
        id: window2;
        objectName: "window2";
        color: "#FF0000";
        width: 100; height: 100;
        Item {
            width: 100; height: 100;
        }
    }
}
