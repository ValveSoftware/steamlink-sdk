import org.qtproject.AutoTestQmlMixedPluginType 1.0
import QtQuick 1.0

Item {
    property bool test: false
    Bar {
        id: bar
    }

    Component.onCompleted: {
        test = (bar.value == 16);
    }
}
