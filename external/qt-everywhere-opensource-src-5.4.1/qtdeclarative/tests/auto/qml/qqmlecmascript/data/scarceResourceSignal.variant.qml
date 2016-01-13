import QtQuick 2.0
import Qt.test 1.0

QtObject {
    id: root

    property MyScarceResourceObject a;
    a: MyScarceResourceObject { id: scarceResourceProvider }

    property ScarceResourceSignalComponentVariant b;
    b: ScarceResourceSignalComponentVariant {
        objectName: "srsc"

        onTestSignal: {
            // this signal will be invoked manually in the test.
            // the scarce resource should be released automatically after evaluation
            // and since we don't keep a copy of it, the pixmap will be detached.
            width = (scarceResourceProvider.scarceResource,10)
        }

        onTestSignal2: {
            // this signal will be invoked manually in the test.
            // the scarce resource should be released automatically after evaluation
            // but since we assign it to a property, the pixmap won't be detached.
            scarceResourceCopy = scarceResourceProvider.scarceResource
        }
    }
}

