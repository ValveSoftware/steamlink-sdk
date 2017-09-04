import QtQml 2.0

QtObject {
    id: root

    property bool success: false
    property bool gotInvalid: false
    property bool expectNull: false
    property SignalEmitter e: SignalEmitter { testObject: root; handleSignal: false } // false so it doesn't use bound handler.

    function determineSuccess(param) {
        if (root.expectNull == true) {
            if (param != null) {
                // the parameter shouldn't have been passed through, but was.
                root.success = false;
                root.gotInvalid = true;
            } else {
                root.success = true;
                root.gotInvalid = false;
            }
            return;
        } else if (param == null) {
            // the parameter should have been passed through, but wasn't.
            root.success = false;
            return;
        }

        if (param.testProperty == 42) {
            // got the expected value.  if we didn't previously
            // get an unexpected value, set success to true.
            root.success = (!root.gotInvalid);
        } else {
             // the value passed through was not what we expected.
            root.gotInvalid = true;
            root.success = false;
        }
    }

    function handleTestSignal(spp) {
        root.determineSuccess(spp);
    }

    Component.onCompleted: {
        success = false;
        e.testSignal.connect(handleTestSignal)
        e.emitTestSignal();
    }
}
