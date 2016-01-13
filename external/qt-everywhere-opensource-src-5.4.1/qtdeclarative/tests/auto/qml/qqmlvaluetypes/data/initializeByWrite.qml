import QtQuick 2.0

ListView {
    property bool test: false

    width: 200
    height: 200
    model: 20

    delegate: Text {
        text: qsTr(props.titleText).arg(index)
        color: props.titleColor
        font.pointSize: props.titlePointSize
    }

    property QtObject props: QtObject {
        property string titleText: "List Item %1 Title"
        property color titleColor: Qt.rgba(1, 0, 0, 0)
        property int titlePointSize: 18
    }

    Component.onCompleted: {
        test = (props.titleText == "List Item %1 Title") &&
               (props.titleColor == Qt.rgba(1, 0, 0, 0)) &&
               (props.titlePointSize == 18)
    }
}
