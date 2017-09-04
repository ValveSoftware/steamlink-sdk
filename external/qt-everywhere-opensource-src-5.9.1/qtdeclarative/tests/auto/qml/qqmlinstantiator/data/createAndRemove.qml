import QtQml 2.1
import QtQuick 2.1

Rectangle {
    Instantiator {
        objectName: "instantiator1"
        model: model1
        delegate: QtObject {
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
