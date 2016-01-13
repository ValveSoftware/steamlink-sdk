import QtQml 2.0
QtObject {
    property SubObject subObject: SubObject {
        subValue: 20;
        property int someExpression: {
            return someValue;
        }
    }
    property int someValue: 42
}
