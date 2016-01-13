import Qt.test 1.0

MyQmlObject{
    id: obj
    objectName: "obj"
    function createOne()
    {
        obj.objectProperty = Qt.createQmlObject('import Qt.test 1.0; MyQmlObject{objectName:"objectOne"}', obj);
    }

    function createTwo()
    {
        var component = Qt.createComponent('dynamicCreation.helper.qml');
        obj.objectProperty = component.createObject(obj);
    }

    function createThree()
    {
        obj.objectProperty = Qt.createQmlObject('TypeForDynamicCreation{}', obj);
    }

    function dontCrash()
    {
        var component = Qt.createComponent('file-doesnt-exist.qml');
        obj.objectProperty = component.createObject(obj);
    }
}
