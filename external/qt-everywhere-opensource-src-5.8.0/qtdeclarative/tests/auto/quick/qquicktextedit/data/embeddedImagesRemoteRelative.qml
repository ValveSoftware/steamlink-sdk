import QtQuick 2.0

TextEdit {
    property string serverBaseUrl;
    textFormat: TextEdit.RichText
    text: "<img src='exists.png'>"
    baseUrl: serverBaseUrl + "/text.html"
}
