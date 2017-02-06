spy = {
    extension: __extension__,
    setupPackage: __setupPackage__,
    postInit: __postInit__
};
__postInit__ = function() { postInitWasCalled = true; };
