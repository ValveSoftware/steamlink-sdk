import QtQuick 2.0

Item {
    property int t1: 1
    property int t2: 2

    function randomNumber() {
        return 4;
    }

    Component.onCompleted: {
        // shouldn't "convert" the randomNumber function into a binding permanently
        t1 = Qt.binding(randomNumber)

        // therefore, the following assignment should fail.
        t2 = randomNumber
   }
}
