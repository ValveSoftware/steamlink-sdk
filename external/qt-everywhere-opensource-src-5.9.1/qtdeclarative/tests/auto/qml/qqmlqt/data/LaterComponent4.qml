import QtQuick 2.0

Item {
    id: deleteme4
    function testFn() {
        // this function shouldn't be called,
        // since the object will have been deleted.
        row.test13_1 = 2;
    }

    Component.onDestruction: row.test13_1 = 1; // success == the object was deleted, but testFn wasn't called.
}
