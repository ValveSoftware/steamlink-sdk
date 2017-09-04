import QtQuick 2.0

TextEdit {
    property string serverBaseUrl;
    textFormat: TextEdit.RichText
    text: "<img src='" + serverBaseUrl + "/notexists.png'>"
}
