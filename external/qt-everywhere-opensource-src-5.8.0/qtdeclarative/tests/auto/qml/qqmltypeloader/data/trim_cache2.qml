import QtQuick 2.0

Item {
    width: 400
    height: 400

    Component.onCompleted: {
        var component = Qt.createComponent("MyComponent.qml")
        if (component.status == Component.Error)
            console.log(component.errorString())
    }
}

