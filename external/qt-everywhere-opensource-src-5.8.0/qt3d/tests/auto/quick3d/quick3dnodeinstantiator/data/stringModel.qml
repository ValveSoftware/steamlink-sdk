import QtQml 2.1
import Qt3D.Core 2.0

Entity {
    NodeInstantiator {
        model: ["alpha", "beta", "gamma", "delta"]
        delegate: Entity {
            property bool success: index == 1 ? datum.length == 4 : datum.length == 5
            property string datum: modelData
        }
    }
}
