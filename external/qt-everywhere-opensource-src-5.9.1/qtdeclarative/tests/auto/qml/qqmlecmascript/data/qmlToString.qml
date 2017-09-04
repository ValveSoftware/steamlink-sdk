import Qt.test 1.0

MyQmlObject{
    id: obj
    objectName: "objName"
    function testToString()
    {
        obj.stringProperty = obj.toString();
    }

}
