import Test 1.0

MyQmlObject {
    property int testData: 9
    property alias testData2: myObject.test1

    readonly property int test1: 10
    readonly property int test2: testData + 9
    readonly property alias test3: myObject.test1


    property variant dummy: MyQmlObject {
        id: myObject
        property int test1: 13
    }
}

