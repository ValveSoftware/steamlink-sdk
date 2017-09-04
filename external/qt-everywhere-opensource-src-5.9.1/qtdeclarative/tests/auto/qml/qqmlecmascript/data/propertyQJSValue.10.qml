import QtQuick 2.0

PropertyQJSValueBaseItem {
    property bool test: false
    Component.onCompleted: {
        test = true;
    }
}
