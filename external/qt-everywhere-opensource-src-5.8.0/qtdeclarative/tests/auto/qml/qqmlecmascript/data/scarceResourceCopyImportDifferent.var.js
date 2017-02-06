.import Qt.test 1.0 as JsQtTest

// In this case, we create the returned scarce resource each call,
// so the object will be different every time it is returned.

var mostRecent

function importScarceResource() {
    var component = Qt.createComponent("scarceResourceCopy.var.qml");
    var scarceResourceElement = component.createObject(null);
    var scarceResourceProvider = scarceResourceElement.a;
    var retn = scarceResourceProvider.scarceResource;
    mostRecent = retn;
    return retn;
}

function destroyScarceResource() {
    mostRecent.destroy();
}