import QtQuick 2.0
import Qt.test 1.0

Item {
    // single url assignment to url list property
    MySequenceConversionObject {
        id: msco1
        objectName: "msco1"
    }

    // single url binding to url list property
    MySequenceConversionObject {
        id: msco2
        objectName: "msco2"
        urlListProperty: "http://qt-project.org/?get%3cDATA%3e";
    }

    // multiple url assignment to url list property
    MySequenceConversionObject {
        id: msco3
        objectName: "msco3"
    }

    // multiple url binding to url list property
    MySequenceConversionObject {
        id: msco4
        objectName: "msco4"
        urlListProperty: [
            "http://qt-project.org/?get%3cDATA%3e",
            "http://qt-project.org/?get%3cDATA%3e"
        ];
    }

    Component.onCompleted: {
        msco1.urlListProperty = "http://qt-project.org/?get%3cDATA%3e";
        msco3.urlListProperty = [
            "http://qt-project.org/?get%3cDATA%3e",
            "http://qt-project.org/?get%3cDATA%3e"
        ];
    }
}
