import QtQuick 2.1
import Test 1.0

TabFence2 {
    objectName: "root"
    focus: true
    width: 800
    height: 600
    TextInput {
        objectName: "item1"
        activeFocusOnTab: true
    }
    Item {
        objectName: "item2"
    }
}
