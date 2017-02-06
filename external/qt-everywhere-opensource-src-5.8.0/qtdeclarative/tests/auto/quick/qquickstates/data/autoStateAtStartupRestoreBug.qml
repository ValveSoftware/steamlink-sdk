import QtQuick 2.0

Item {
    id: root
    property int input: 1
    property int test: 9

    states: [
        State {
            name: "portrait"
            when: root.input == 1
            PropertyChanges {
                target: root
                test: 3
            }
        }
    ]
}
