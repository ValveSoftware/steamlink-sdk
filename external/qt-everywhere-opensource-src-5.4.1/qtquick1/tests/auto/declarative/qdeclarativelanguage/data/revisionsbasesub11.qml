import QtQuick 1.0
import Test 1.1

MyRevisionedSubclass
{
    propA: 10
    propB: 10
    propC: 10
    // propD is not registered in 1.1
    prop1: 10
    prop2: 10
    prop3: 10
    prop4: 10

    onSignal4: prop4 = 2
}
