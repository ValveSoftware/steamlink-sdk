import QtQuick 2.0

Rectangle {
    width: 1024
    height: 768

    Item {
        id: area
        objectName: "area"
        property int numx: 6
        property int cellwidth: 1024/numx

        onWidthChanged: {
            width = width>1024?1024:width;
        }

        state: 'minimal'
        states: [
            State {
                name: 'minimal'
                PropertyChanges {
                    target: area
                    width: cellwidth
                }
            }
        ]

    }
}
