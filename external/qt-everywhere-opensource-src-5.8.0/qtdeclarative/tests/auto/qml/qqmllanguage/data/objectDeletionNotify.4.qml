import QtQuick 2.0

Item {
    property bool success: false

    Component {
        id: internal

        Item {
        }
    }

    property var expectNull: null

    function setExpectNull(b) {
        success = false;
        expectNull = b;
    }

    function setExpectNoChange() {
        success = true;
        expectNull = null;
    }

    property var obj: null
    onObjChanged: success = (expectNull == null) ? false : (expectNull ? obj == null : obj != null)

    property var temp: null

    Component.onCompleted: {
        // Set obj to contain an object
        setExpectNull(false)
        obj = internal.createObject(null, {})
        if (!success) return

        // Use temp variable to keep object reference alive
        temp = obj

        // Change obj to contain a string
        setExpectNull(false)
        obj = 'hello'
    }

    function destroyObject() {
        setExpectNoChange()
        temp.destroy();
    }
}
