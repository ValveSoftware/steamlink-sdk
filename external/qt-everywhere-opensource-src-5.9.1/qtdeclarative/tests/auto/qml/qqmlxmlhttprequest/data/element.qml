import QtQuick 2.0

QtObject {
    property bool xmlTest: false
    property bool dataOK: false

    function checkElement(e, person, fruit)
    {
        if (e.tagName != "root")
            return;

        if (e.nodeName != "root")
            return;

        if (e.nodeValue != null)
            return;

        if (e.nodeType != 1)
            return;

        var childTagNames = [ "person", "fruit" ];

        if (e.childNodes.length != childTagNames.length)
            return;

        for (var ii = 0; ii < childTagNames.length; ++ii) {
            if (e.childNodes[ii].tagName != childTagNames[ii])
                return;
        }

        if (e.childNodes[childTagNames.length + 1] != null)
            return;

        // Check writing fails
        e.childNodes[0] = null;
        if (e.childNodes[0] == null)
            return;

        e.childNodes[10] = 10;
        if (e.childNodes[10] != null)
            return;

        if (e.firstChild.tagName != e.childNodes[0].tagName)
            return;

        if (e.lastChild.tagName != e.childNodes[1].tagName)
            return;

        if (e.previousSibling != null)
            return;

        if (e.nextSibling != null)
            return;

        if (e.attributes == null)
            return;

        if (e.attributes.length != 2)
            return;

        var attr1 = e.attributes["attr"];
        if (attr1.nodeValue != "value")
            return;

        var attrIdx = e.attributes[0];
        if (attrIdx.nodeValue != "value")
            return;

        var attr2 = e.attributes["attr2"];
        if (attr2.nodeValue != "value2")
            return;

        var attr3 = e.attributes["attr3"];
        if (attr3 != null)
            return;

        var attrIdx2 = e.attributes[11];
        if (attrIdx2 != null)
            return;

        // Check writing fails
        e.attributes[0] = null;
        if (e.attributes[0] == null)
            return;

        e.attributes["attr"] = null;
        if (e.attributes["attr"] == null)
            return;

        e.attributes["attr3"] = 10;
        if (e.attributes["attr3"] != null)
            return;

        // Check person and fruit sub elements
        if (person.parentNode.nodeName != "root")
            return;

        if (person.previousSibling != null)
            return;

        if (person.nextSibling.nodeName != "fruit")
            return;

        if (fruit.parentNode.nodeName != "root")
            return;

        if (fruit.previousSibling.nodeName != "person")
            return;

        if (fruit.nextSibling != null)
            return;

        xmlTest = true;
    }

    function checkXML(document)
    {
        checkElement(document.documentElement,
                     document.documentElement.childNodes[0],
                     document.documentElement.childNodes[1]);
    }

    Component.onCompleted: {
        var x = new XMLHttpRequest;

        x.open("GET", "element.xml");

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



