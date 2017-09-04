import Qt.test 1.0

MyQmlObject {
    resettableProperty: 19

    function doReset() {
        resettableProperty = undefined;
    }
}

