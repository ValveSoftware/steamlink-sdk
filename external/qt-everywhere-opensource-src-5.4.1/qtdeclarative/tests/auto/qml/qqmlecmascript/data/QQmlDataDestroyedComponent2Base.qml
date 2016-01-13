import QtQuick 2.0
import Qt.test.qobjectApi 1.0 as ModApi

Rectangle {
    id: base
    x: 1
    color: "red"
    property bool testConditionsMet: false

    onXChanged: {
        ModApi.QObject.trackObject(base);
        ModApi.QObject.trackedObject(); // flip the ownership.
        if (!ModApi.QObject.trackedObjectHasJsOwnership())
            testConditionsMet = false;
        else
            testConditionsMet = true;
   }

    onColorChanged: {
        // will be triggered during beginCreate of derived
        test.testConditionsMet = testConditionsMet;
        gc();
        gc();
        gc();
    }
}
