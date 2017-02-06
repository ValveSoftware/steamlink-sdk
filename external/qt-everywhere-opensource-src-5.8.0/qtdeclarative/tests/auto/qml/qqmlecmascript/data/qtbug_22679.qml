import QtQuick 2.0

QtObject {
    function accessContextProperty() {
        for (var i = 0; i < contextProp.stringProperty.length; ++i) ;
    }

    Component.onCompleted: {
        for (var i = 0; i < 1000; ++i)
            accessContextProperty();
        // Shouldn't cause "Illegal invocation" error.
        gc();
    }
}
