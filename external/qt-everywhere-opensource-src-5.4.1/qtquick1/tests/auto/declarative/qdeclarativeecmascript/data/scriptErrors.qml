import Qt.test 1.0
import "scriptErrors.js" as Script

MyQmlObject {
    property int t: a.value
    property int w: Script.getValue();
    property int x: undefined
    property int y: (a.value, undefinedObject)

    onBasicSignal: { console.log(a.value); }
    id: myObj
    onAnotherBasicSignal: myObj.trueProperty = false;
    onThirdBasicSignal: myObj.fakeProperty = "";
}

