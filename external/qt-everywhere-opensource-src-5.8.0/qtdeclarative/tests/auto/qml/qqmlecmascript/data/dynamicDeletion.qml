import Qt.test 1.0

MyQmlObject{
    id: obj
    objectName: "obj"
    function create()
    {
        obj.objectProperty = Qt.createQmlObject('import Qt.test 1.0; MyQmlObject{objectName:"emptyObject"}', obj);
    }

    function killOther()
    {
        obj.objectProperty.destroy(500);
    }

    function killMe()
    {
        obj.destroy();//Must not segfault
    }
}
