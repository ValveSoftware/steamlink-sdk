import QtQml 2.0

QtObject {
    property bool someProperty: true
    function testCall() {
        try {
            someProperty(); // should throw
            return false
        } catch (e) {
            return true
        }
    }
}
