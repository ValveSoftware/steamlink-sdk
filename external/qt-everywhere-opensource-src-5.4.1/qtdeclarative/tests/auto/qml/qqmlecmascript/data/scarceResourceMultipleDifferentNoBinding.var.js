.import Qt.test 1.0 as JsQtTest

function importScarceResource() {
    var component = Qt.createComponent("scarceResourceCopy.var.qml");
    var scarceResourceElement = component.createObject(null);
    var scarceResourceProvider = scarceResourceElement.a;
    var retn = scarceResourceProvider.scarceResource;
    retn.preserve();
    return retn;
}

function releaseScarceResource(resource) {
    resource.destroy();
}