import QtQml 2.0
QtObject {
    property bool somePropertyEvaluated: false;

    property int someProperty: {
        // It's sort of evil to set the property here, but that doesn't mean that
        // this expression should get re-evaluated when unrelatedProperty changes later.
        somePropertyEvaluated = true
        return 20;
    }
    Component.onCompleted: {
        somePropertyEvaluated = false
    }
}
