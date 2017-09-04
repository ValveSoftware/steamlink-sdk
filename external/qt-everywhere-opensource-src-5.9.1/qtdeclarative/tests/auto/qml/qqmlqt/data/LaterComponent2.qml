import QtQuick 2.0

Item {
    id: deleteme2
    function testFn() {
        // this function shouldn't be called,
        // since the object will have been deleted.
        var crashy = Qt.createQmlObject("import QtQuick 2.0; Item { }", deleteme2) // invalid calling context if invoked after gc
        row.test11_1 = 2;
    }

    Component.onDestruction: row.test11_1 = 1; // success == the object was deleted, but testFn wasn't called.
}
