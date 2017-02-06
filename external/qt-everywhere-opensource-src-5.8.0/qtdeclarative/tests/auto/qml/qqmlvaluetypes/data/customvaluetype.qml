import QtQuick 2.0
import Test 1.0

TypeWithCustomValueType {
    desk {
        monitorCount: 3
    }
    derivedGadget {
        baseProperty: 42
    }
}
