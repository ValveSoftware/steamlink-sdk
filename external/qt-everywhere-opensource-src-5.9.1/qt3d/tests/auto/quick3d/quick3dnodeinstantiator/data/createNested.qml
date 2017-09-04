import QtQml 2.1
import Qt3D.Core 2.0

Entity {
    NodeInstantiator {
        model: 3
        delegate: NodeInstantiator {
            model: 4
            delegate: Entity {
                property bool success: true
                property int idx: index
            }
        }
    }
}
