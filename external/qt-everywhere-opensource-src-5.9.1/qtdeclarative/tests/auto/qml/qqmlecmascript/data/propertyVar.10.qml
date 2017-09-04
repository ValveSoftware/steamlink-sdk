import QtQuick 2.0

PropertyVarBaseItem {
    property bool test: false
    Component.onCompleted: {
        test = true;
    }
}
