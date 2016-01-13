import QtQuick 2.0

Rectangle {
    function something(i) {
        if (i > 10) {
            something(i / 4);
        } else {
            console.log("done");
        }
    }

    width: 400
    height: 400

    onWidthChanged: something(width);
    Component.onCompleted: width = 500;
}
