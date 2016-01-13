import QtQuick 1.0
import Qt.test 1.0

QtObject {
    property int value: 9
    property int value2

    MyQmlObject.onMySignal: value2 = value
}
