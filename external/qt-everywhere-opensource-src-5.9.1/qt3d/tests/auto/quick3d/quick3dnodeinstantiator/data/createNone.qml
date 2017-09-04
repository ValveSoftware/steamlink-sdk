import QtQml 2.1
import Qt3D.Core 2.0

Entity {
    NodeInstantiator {
        model: 0
        property bool success: true
        Entity {
            property bool success: true
            property int idx: index
        }
        onObjectChanged: success = false;//Don't create intermediate objects
        onCountChanged: success = false;//Don't create intermediate objects
    }
}
