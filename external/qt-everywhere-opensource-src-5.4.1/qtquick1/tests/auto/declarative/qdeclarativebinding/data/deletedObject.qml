import QtQuick 1.0

Rectangle {
    id: wrapper
    width: 400
    height: 400

    property bool activateBinding: false

    Binding {
        id: binding
        target: Qt.createQmlObject('import QtQuick 1.0; Item { property real value: 10 }', wrapper)
        property: "value"
        when: activateBinding
        value: x + y
    }

    Component.onCompleted: binding.target.destroy();

//    MouseArea {
//        anchors.fill: parent
//        onClicked: activateBinding = true;
//    }
}
