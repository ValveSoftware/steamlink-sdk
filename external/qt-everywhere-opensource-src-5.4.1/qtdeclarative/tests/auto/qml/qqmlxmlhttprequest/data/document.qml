import QtQuick 2.0

QtObject {
    property bool xmlTest: false
    property bool dataOK: false

    function checkXML(document)
    {
        if (document.xmlVersion != "1.0")
            return;

        if (document.xmlEncoding != "UTF-8")
            return;

        if (document.xmlStandalone != true)
            return;

        if (document.documentElement == null)
            return;

        if (document.nodeName != "#document")
            return;

        if (document.nodeValue != null)
            return;

        if (document.parentNode != null)
            return;

        // ### Test other node properties
        // ### test encoding (what is a valid qt encoding?)
        xmlTest = true;
    }

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", "document.xml");

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {

                dataOK = true;

                if (x.responseXML != null)
                    checkXML(x.responseXML);

            }
        }

        x.send()
    }
}


