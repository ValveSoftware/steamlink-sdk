import QtQuick 2.1
import QtQuick.Window 2.1

Window {
    id: window1;
    objectName: "window1";
    width: 100; height: 100;
    visible: true
    color: "blue"
    property alias win2: window2
    property alias win3: window3
    property alias win4: window4
    property alias win5: window5
    Window {
        id: window2;
        objectName: "window2";
        width: 100; height: 100;
        visible: true
        color: "green"
        Window {
            id: window3;
            objectName: "window3";
            width: 100; height: 100;
            visible: true
        }

        Window { //Is invisible by default
            id: window4
            objectName: "window4";
            height: 200
            width: 200
            color: "black"
            Window {
                id: window5
                objectName: "window5";
                height: 200
                width: 200
                visible: true
            }
        }
    }
}
