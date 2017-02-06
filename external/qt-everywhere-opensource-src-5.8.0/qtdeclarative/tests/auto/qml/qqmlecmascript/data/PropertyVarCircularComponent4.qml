import QtQuick 2.0

Rectangle {
    id: rectangle      // will have JS ownership
    objectName: "rectangle"
    width: 10
    height: 10
    property var rectCanary: 5

    Text {
        id: text // will have Eventual-JS ownership
        objectName: "text"
        property var vp
        property var textCanary: 10

        // The varProperties array of "text" is weak
        // (due to eventual JS ownership since parent is JS owned)
        // but nonetheless, the reference to the created QObject
        // should cause that QObject to NOT be collected.
        function constructQObject() {
            var component = Qt.createComponent("PropertyVarCircularComponent5.qml");
            if (component.status == Component.Ready) {
                text.vp = component.createObject(null); // has JavaScript ownership
            }
            gc();
        }
    }
}
