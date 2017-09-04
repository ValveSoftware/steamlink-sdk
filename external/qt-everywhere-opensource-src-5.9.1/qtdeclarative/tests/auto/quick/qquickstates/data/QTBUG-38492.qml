import QtQuick 2.0

Item {
    id: root
    property string text;

    states: [
        State {
            name: 'apply'
            PropertyChanges {
                target: root
                text: qsTr("Test")
            }
        }
    ]
}
