import QtQuick 2.0
import Qt.test 1.0

Item {
    property bool test: false

    MyQmlObject { id: nullOneObj; qjsvalue: null }
    MyQmlObject { id: nullTwoObj; }
    MyQmlObject { id: undefOneObj; qjsvalue: undefined }
    MyQmlObject { id: undefTwoObj }

    property alias nullOne: nullOneObj.qjsvalue
    property alias nullTwo: nullTwoObj.qjsvalue
    property alias undefOne: undefOneObj.qjsvalue
    property alias undefTwo: undefTwoObj.qjsvalue

    Component.onCompleted: {
        nullTwo = null;
        undefTwo = undefined;
        if (nullOne != null) return;
        if (nullOne != nullTwo) return;
        if (undefOne != undefined) return;
        if (undefOne != undefTwo) return;
        test = true;
    }
}
