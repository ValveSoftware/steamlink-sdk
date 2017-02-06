import QtQuick 2.0
import Qt.test 1.0

Item {
    property int testValue: 3499
    SelfRegistering {
        property int testValue2: 19
    }
}
