import QtQuick 2.0
import "createComponent_lib.js" as Test

Item {
    property int status: Component.Null
    property int readValue: 0

    Component.onCompleted: {
        status = Test.loadComponent()
        readValue = Test.createComponent().test;
    }
}
