import QtQuick 2.0

Loader {
    id: myLoader
    property int testProperty: 1912
    sourceComponent: loaderComponent
    Component {
        id: loaderComponent
        Item {
            Component.onCompleted: {
                test = (myLoader.testProperty == 1912);
            }
        }
    }
}
