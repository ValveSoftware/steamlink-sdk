import QtQml 2.0
QtObject {
    property int testValue: -1
    property int subValue;
    onSubValueChanged: {
        testValue = this.someExpression
    }
}
