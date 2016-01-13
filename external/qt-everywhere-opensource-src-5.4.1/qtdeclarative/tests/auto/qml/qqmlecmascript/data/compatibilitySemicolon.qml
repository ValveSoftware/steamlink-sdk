
import QtQuick 2.0

QtObject {
    function code() {

        // Not correct according to ECMA 5.1, but JSC and V8 accept the following:
        do {
            ;
        } while (false) true
    }
}

