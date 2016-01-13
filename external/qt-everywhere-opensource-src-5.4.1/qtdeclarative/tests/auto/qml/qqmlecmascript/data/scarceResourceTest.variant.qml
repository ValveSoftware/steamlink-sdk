import QtQuick 2.0
import Qt.test 1.0

// Here we import a scarce resource directly, and use it in a binding.
// It is not preserved or released manually, so it should be
// automatically released once evaluation of the binding
// is complete.

QtObject {
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
    property int scarceResourceTest: scarceResourceProvider.scarceResource,100 // return 100, but include the scarceResource in the binding to be evaluated.
}

