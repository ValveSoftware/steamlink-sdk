.import Qt.test 1.0 as JsQtTest

// In this case, the "retn" variable will be evaluated during import.
// Since the importScarceResource() function depends on this variable,
// because we DO NOT call "retn.preserve()", the scarce resource will
// be released after the import completes but prior to evaluation of
// any binding which calls "importScarceResource()".
// Thus, "importScarceResource()" will return a released (invalid)
// scarce resource.

var component = Qt.createComponent("scarceResourceCopy.variant.qml");
var scarceResourceElement = component.createObject(null);
var scarceResourceProvider = scarceResourceElement.a;
var retn = scarceResourceProvider.scarceResource;

function importScarceResource() {
    return retn; // should return a released (invalid) scarce resource
}

