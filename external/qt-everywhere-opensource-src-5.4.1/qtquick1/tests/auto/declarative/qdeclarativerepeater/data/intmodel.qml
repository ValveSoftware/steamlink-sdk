import QtQuick 1.0

Rectangle {
    id: container
    objectName: "container"
    width: 240
    height: 320
    color: "white"

    function checkProperties() {
        testObject.error = false;
        if (repeater.delegate != comp) {
            console.log("delegate property incorrect");
            testObject.error = true;
        }
    }

    Component {
        id: comp
        Item{}
    }

    Repeater {
        id: repeater
        objectName: "repeater"
        model: testData
        delegate: comp
    }
}
