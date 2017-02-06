.import Qt.test 1.0 as JsQtTest

function importScarceResource(scarceResourceProvider) {
    // the scarce resource should be automatically released
    // after the binding is evaluated if preserve is not
    // called.
    return scarceResourceProvider.scarceResource;
}

function importPreservedScarceResource(scarceResourceProvider) {
    // the scarce resource is manually preserved
    // during the evaluation of the binding.
    // it should not be released.
    var scarceResource = scarceResourceProvider.scarceResource;
    scarceResource.preserve();
    return scarceResource;
}

function importReleasedScarceResource(scarceResourceProvider) {
    // release the scarce resource during the
    // evaluation of the binding.  The returned
    // variant will therefore be invalid.
    var scarceResource = scarceResourceProvider.scarceResource;
    scarceResource.destroy();
    return scarceResource;
}

function importPreservedScarceResourceFromMultiple(scarceResourceProvider) {
    // some scarce resources are manually preserved,
    // some of them are manually destroyed,
    // and some are automatically managed.
    // We return a preserved resource
    var sr1 = scarceResourceProvider.scarceResource; // preserved/destroyed.
    sr1.preserve();
    var sr2 = scarceResourceProvider.scarceResource; // preserved/destroyed
    sr2.preserve();
    var sr3 = scarceResourceProvider.scarceResource; // automatic.
    var sr4 = scarceResourceProvider.scarceResource; // automatic and returned.
    var sr5 = scarceResourceProvider.scarceResource; // destroyed
    sr5.destroy();
    sr2.destroy();
    var sr6 = scarceResourceProvider.scarceResource; // destroyed
    var sr7 = scarceResourceProvider.scarceResource; // automatic
    sr1.destroy();
    sr6.destroy();
    return sr4;
}

