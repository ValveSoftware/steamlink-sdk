import QtQuick 2.0
import Qt.test 1.0

MyQmlObject {
    id: first
    property bool aliasedIntIsUndefined: false
    property alias intAlias: objprop.intp

    objectProperty: QtObject {
        id: objprop
        property int intp: 12
    }

    function resetAlias() {
        intAlias = undefined; // should error
        aliasedIntIsUndefined = (objprop.intp == undefined);
    }
}
