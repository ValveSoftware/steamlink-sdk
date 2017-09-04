import QtQuick 2.1
import Test 1.0

Item {
    objectName: "root"
    focus: true
    width: 800
    height: 600

    TabFence {
        objectName: "fence1"

        TextInput {
            objectName: "input11"
            activeFocusOnTab: true
        }
        TextInput {
            objectName: "input12"
            activeFocusOnTab: true
        }
        TextInput {
            objectName: "input13"
            activeFocusOnTab: true
        }
    }

    TextInput {
        objectName: "input1"
        activeFocusOnTab: true
    }

    TextInput {
        objectName: "input2"
        activeFocusOnTab: true
    }

    TabFence {
        objectName: "fence2"
    }

    TextInput {
        objectName: "input3"
        activeFocusOnTab: true
    }

    TabFence {
        objectName: "fence3"
    }
}
