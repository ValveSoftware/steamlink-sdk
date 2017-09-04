.import Qt.test 1.0 as JsQtTest

var component = Qt.createComponent("scarceResourceCopy.var.qml");
var scarceResourceElement = component.createObject(null);
var scarceResourceProvider = scarceResourceElement.a;
var retn = scarceResourceProvider.scarceResource;
retn.preserve();

function importScarceResource() {
    return retn;
}

function releaseScarceResource(resource) {
    resource.destroy();
}