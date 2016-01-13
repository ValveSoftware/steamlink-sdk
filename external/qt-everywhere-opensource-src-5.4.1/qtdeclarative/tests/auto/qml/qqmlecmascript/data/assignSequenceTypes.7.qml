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
        urlListProperty: "example.html"
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
        urlListProperty: [ "example.html", "example2.html" ]
    }

    // multiple url binding to url list property - already resolved
    MySequenceConversionObject {
        id: msco5
        objectName: "msco5"
        urlListProperty: [ Qt.resolvedUrl("example.html"), Qt.resolvedUrl("example2.html") ]
    }

    Component.onCompleted: {
        msco1.urlListProperty = "example.html";
        msco3.urlListProperty = [ "example.html", "example2.html" ];
    }
}
