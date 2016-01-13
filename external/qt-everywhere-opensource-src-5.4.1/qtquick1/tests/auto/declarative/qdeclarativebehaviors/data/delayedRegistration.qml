import QtQuick 1.0

Rectangle {
    id: container

    width: 400; height: 400;
    property Item myItem

    function doCreate() {
        myItem = myComponent.createObject(container)
        myItem.x = 100
    }

    Component {
        id: myComponent
        Rectangle {
            width: 100
            height: 100
            color: "green"
            Behavior on x { NumberAnimation { duration: 500 } }
        }
    }

    Component.onCompleted: doCreate()
}
