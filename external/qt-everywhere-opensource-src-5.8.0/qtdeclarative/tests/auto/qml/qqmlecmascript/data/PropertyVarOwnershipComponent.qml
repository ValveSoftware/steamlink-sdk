import QtQuick 2.0

Rectangle {
    id: rectangle      // will have JS ownership
    objectName: "rectangle"
    width: 10
    height: 10
    property var rectCanary: 5

    Text {
        id: textOne // will have Eventual-JS ownership
        objectName: "textOne"
        property var textCanary: 11
        property var vp
    }

    Text {
        id: textTwo
        objectName: "textTwo"
        property var textCanary: 12
        property var vp

        function constructQObject() {
            var component = Qt.createComponent("PropertyVarCircularComponent5.qml");
            if (component.status == Component.Ready) {
                textTwo.vp = component.createObject(null); // has JavaScript ownership
            }
            gc();
        }

        function deassignVp() {
            textTwo.textCanary = 22;
            textTwo.vp = textTwo.textCanary;
        }
    }
}
