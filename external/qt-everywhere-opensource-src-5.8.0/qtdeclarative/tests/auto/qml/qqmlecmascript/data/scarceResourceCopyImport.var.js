.import Qt.test 1.0 as JsQtTest

// In this case, the "retn" variable will be evaluated during import.
// Since the "importScarceResource()" function depends on this variable,
// we must explicitly preserve the "retn" variable or the scarce
// resource would automatically be released after import completes
// but before the binding is evaluated.

var component = Qt.createComponent("scarceResourceCopy.var.qml");
var scarceResourceElement = component.createObject(null);
var scarceResourceProvider = scarceResourceElement.a;
var retn = scarceResourceProvider.scarceResource;
retn.preserve(); // must preserve manually or it will be released!

function importScarceResource() {
    // if called prior to calling destroyScarceResource(),
    // this function should return the preserved scarce resource.
    // otherwise, it should return an invalid variant.
    return retn;
}

function destroyScarceResource() {
    retn.destroy();
}

