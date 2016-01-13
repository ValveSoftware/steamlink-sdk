import QtQuick 2.0

QtObject {
    property bool test: false
    Component.onCompleted: {
        try {
            console.log(sorryNoSuchProperty);
        } catch (e) {
            test = true;
        }
    }
}
