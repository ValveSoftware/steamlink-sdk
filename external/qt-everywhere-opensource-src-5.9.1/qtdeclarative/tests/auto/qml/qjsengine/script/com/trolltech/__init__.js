var wasDefinedAlready = (com["trolltech"] != undefined);
__setupPackage__("com.trolltech");
com.trolltech.wasDefinedAlready = wasDefinedAlready;
com.trolltech.name = __extension__;
com.trolltech.level = com.level + 1;

com.trolltech.postInitCallCount = 0;
com.trolltech.originalPostInit = __postInit__;
__postInit__ = function() { ++com.trolltech.postInitCallCount; };
