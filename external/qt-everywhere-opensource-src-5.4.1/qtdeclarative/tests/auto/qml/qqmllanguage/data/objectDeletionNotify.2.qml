import QtQuick 2.0

Item {
    property bool success: false

    Component {
        id: internal

        Item {
        }
    }

    property bool expectNull: null

    function setExpectNull(b) {
        success = false;
        expectNull = b;
    }

    property variant obj: null
    onObjChanged: success = (expectNull ? obj == null : obj != null)

    Component.onCompleted: {
        setExpectNull(false)
        obj = internal.createObject(null, {})
        if (!success) return

        // Replace with a different object
        setExpectNull(false)
        obj = internal.createObject(null, {})
    }

    function destroyObject() {
        setExpectNull(true)
        obj.destroy();
    }
}
