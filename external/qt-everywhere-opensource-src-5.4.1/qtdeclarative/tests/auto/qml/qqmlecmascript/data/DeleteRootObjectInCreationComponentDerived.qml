import QtQuick 2.0

DeleteRootObjectInCreationComponentBase {
    id: derived
    color: "black" // will trigger change signal during beginCreate.

    property bool testConditionsMet: false // will be set by base
    function setTestConditionsMet(obj) {
        obj.testConditionsMet = derived.testConditionsMet;
    }
}
