import QtQml 2.1

Instantiator {
    model: ["alpha", "beta", "gamma", "delta"]
    delegate: QtObject {
        property bool success: index == 1 ? datum.length == 4 : datum.length == 5
        property string datum: modelData
    }
}
