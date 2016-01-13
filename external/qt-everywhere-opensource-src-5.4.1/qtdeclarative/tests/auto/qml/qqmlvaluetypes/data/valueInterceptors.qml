import Test 1.0

MyTypeObject {
    property int value: 13;

    MyOffsetValueInterceptor on rect.x {}
    rect.x: value
}
