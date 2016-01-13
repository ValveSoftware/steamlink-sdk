var wasDefinedAlready = (this["com"] != undefined);
__setupPackage__("com");
com.wasDefinedAlready = wasDefinedAlready;
com.name = __extension__;
com.level = 1;

com.postInitCallCount = 0;
com.originalPostInit = __postInit__;
__postInit__ = function() { ++com.postInitCallCount; };
