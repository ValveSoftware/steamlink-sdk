import QtQuick 1.0

QtObject {
    property bool dataOK: false

    property string fileName
    property string responseText
    property string responseXmlRootNodeValue

    function startRequest() {
        var x = new XMLHttpRequest;

        x.open("GET", fileName);

        // Test to the end
        x.onreadystatechange = function() {
            if (x.readyState == XMLHttpRequest.DONE) {

                responseText = x.responseText
                if (x.responseXML)
                    responseXmlRootNodeValue = x.responseXML.documentElement.childNodes[0].nodeValue

                dataOK = true;
            }
        }
        x.send()
    }
}

