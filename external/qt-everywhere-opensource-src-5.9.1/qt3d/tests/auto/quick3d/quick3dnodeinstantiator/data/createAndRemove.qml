import QtQml 2.1
import Qt3D.Core 2.0

Entity {
    NodeInstantiator {
        objectName: "instantiator1"
        model: model1
        delegate: Entity {
            property string datum: model.text
        }
    }
    Component.onCompleted: {
        model1.add("Delta");
        model1.add("Gamma");
        model1.add("Beta");
        model1.add("Alpha");
    }
}
