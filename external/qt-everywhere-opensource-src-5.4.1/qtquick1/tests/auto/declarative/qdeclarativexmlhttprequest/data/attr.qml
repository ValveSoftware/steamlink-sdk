import QtQuick 1.0

QtObject {
    property bool xmlTest: false
    property bool dataOK: false

    function checkAttr(documentElement, attr)
    {
        if (attr == null)
            return;

        if (attr.name != "attr")
            return;

        if (attr.value != "myvalue")
            return;

        if (attr.ownerElement.tagName != documentElement.tagName)
            return;

        if (attr.nodeName != "attr")
            return;

        if (attr.nodeValue != "myvalue")
            return;

        if (attr.nodeType != 2)
            return;

        if (attr.childNodes.length != 0)
            return;

        if (attr.firstChild != null)
            return;

        if (attr.lastChild != null)
            return;

        if (attr.previousSibling != null)
            return;

        if (attr.nextSibling != null)
            return;

        if (attr.attributes != null)
            return;

        xmlTest = true;
    }

    function checkXML(document)
    {
        checkAttr(document.documentElement, document.documentElement.attributes[0]);
    }

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", "attr.xml");

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



