import Test 1.0
import QtQuick 2.0

QtObject {
    id: root
    property int value: 10
    property alias valueAlias: root.value
}
