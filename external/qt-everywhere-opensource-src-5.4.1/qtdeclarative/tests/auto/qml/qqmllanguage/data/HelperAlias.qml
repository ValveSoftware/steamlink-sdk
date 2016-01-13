import Test 1.0
import QtQuick 2.0

QtObject {
    property variant child
    child: QtObject { id: obj }
    property alias objAlias: obj;
}

