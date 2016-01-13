import Test 1.0
import QtQuick 1.0

MyContainer {
    QtObject {
        property int index: 0
    }

    QtObject {
        property int index: 1
    }

    children: [
        QtObject {
            property int index: 2
        },
        QtObject {
            property int index: 3
        }
    ]

    QtObject {
        property int index: 4
    }

    QtObject {
        property int index: 5
    }
}
