import QtQuick 1.0

AliasPropertyChangeSignalsType {
    id: root
    onAliasPropertyChanged: root.test = true

    function blah() {}
    property int a
}

