import QtQml 2.1

Instantiator {
    model: 0
    property bool success: true
    QtObject {
        property bool success: true
        property int idx: index
    }
    onObjectChanged: success = false;//Don't create intermediate objects
    onCountChanged: success = false;//Don't create intermediate objects
}
