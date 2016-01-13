import QtQuick 2.0
import Qt.test 1.0

QtObject {
    // this component doesn't bind any property to a scarce
    // resource from the scarce resource provider,
    // so the binding evaluation resource cleanup
    // codepath shouldn't be activated; so if the resources
    // are released, it will be due to the import evaluation
    // resource cleanup codepath being activated correctly.
    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }
}

