import QtQuick 2.0

Item {
    property int changeCount: 0

    property bool normalName: false
    property bool _nameWithUnderscore: false
    property bool ____nameWithUnderscores: false

    onNormalNameChanged: {
        changeCount = changeCount + 1;
    }

    on_NameWithUnderscoreChanged: {
        changeCount = changeCount + 2;
    }

    on____NameWithUnderscoresChanged: {
        changeCount = changeCount + 3;
    }

    Component.onCompleted: {
        normalName = true;
        _nameWithUnderscore = true;
        ____nameWithUnderscores = true;
    }
}
