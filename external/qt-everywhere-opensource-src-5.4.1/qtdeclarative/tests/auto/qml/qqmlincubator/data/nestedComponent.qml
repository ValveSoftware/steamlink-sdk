import QtQuick 2.0
import "nestedComponent.js" as NestedJS

QtObject {
    property Component c: Component {
        QtObject {
            property int value: NestedJS.value
        }
    }
}
