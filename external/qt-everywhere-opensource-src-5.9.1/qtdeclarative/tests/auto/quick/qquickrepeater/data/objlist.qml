import QtQuick 2.0

Rectangle {
    id: container
    objectName: "container"
    width: 240
    height: 320
    color: "white"
    Repeater {
        id: repeater
        objectName: "repeater"
        model: testData
        property int errors: 0
        property int instantiated: 0
        Component {
            Item{
                Component.onCompleted: {if(index!=modelData.idx) repeater.errors += 1; repeater.instantiated++}
            }
        }
    }
}
