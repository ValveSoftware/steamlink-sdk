import Qt.test 1.0
import QtQuick 1.0

import "functionAssignment.js" as Script

MyQmlObject {
    property variant a
    property int aNumber: 10

    property bool assignToProperty: false
    property bool assignToPropertyFromJsFile: false

    property bool assignWithThis: false
    property bool assignWithThisFromJsFile: false

    property bool assignToValueType: false

    property bool assignFuncWithoutReturn: false
    property bool assignWrongType: false
    property bool assignWrongTypeToValueType: false


    onAssignToPropertyChanged: {
        function myFunction() {
            return aNumber * 10;
        }
        a = myFunction;
    }

    property QtObject obj: QtObject {
        property int aNumber: 4212
        function myFunction() {
            return this.aNumber * 10;   // should use the aNumber from root, not this object
        }
    }
    onAssignWithThisChanged: {
        a = obj.myFunction;
    }

    onAssignToPropertyFromJsFileChanged: {
        Script.bindPropertyWithThis()
    }

    onAssignWithThisFromJsFileChanged: {
        Script.bindProperty()
    }

    property Text text: Text { }
    onAssignToValueTypeChanged: {
        text.font.pixelSize = (function() { return aNumber * 10; })
        a = (function() { return text.font.pixelSize; })
    }


    // detecting errors:

    onAssignFuncWithoutReturnChanged: {
        function myFunction() {
        }
        a = myFunction;
    }

    onAssignWrongTypeChanged: {
        function myFunction() {
            return 'a string';
        }
        aNumber = myFunction;
    }

    onAssignWrongTypeToValueTypeChanged: {
        text.font.pixelSize = (function() { return 'a string'; })
    }
}
