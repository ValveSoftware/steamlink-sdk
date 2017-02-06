import QtQml 2.0

QtObject {
    function something(i) {
        if (i > 10) {
            something(i / 4);
        } else {
            console.log("done");
        }
    }

    property int width: 400
    property int height: 400

    onWidthChanged: something(width);
    Component.onCompleted: width = 500;
}
