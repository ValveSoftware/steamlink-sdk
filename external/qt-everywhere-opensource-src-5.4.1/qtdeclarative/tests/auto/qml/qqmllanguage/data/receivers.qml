import Test 1.0

MyReceiversTestObject {
    property int dummy: prop
    onPropChanged: { var a = 0; }  //do nothing
    onMySignal: { var a = 0; }  //do nothing
}

