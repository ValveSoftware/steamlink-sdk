function bindProperty()
{
    a = Qt.binding(function(){ return aNumber * 10 })
}


function TestObject() { }
TestObject.prototype.aNumber = 928349
TestObject.prototype.bindFunction = function() {
    return this.aNumber * 10        // this should not use the TestObject's aNumber
}
var testObj = new TestObject()

function bindPropertyWithThis()
{
    a = Qt.binding(testObj.bindFunction)
}
