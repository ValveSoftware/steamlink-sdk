import QtQuick 2.0

Item {
    focus: true

    Keys.forwardTo: [ item2 ]

    TextInput {
        id: item2
    }
}
