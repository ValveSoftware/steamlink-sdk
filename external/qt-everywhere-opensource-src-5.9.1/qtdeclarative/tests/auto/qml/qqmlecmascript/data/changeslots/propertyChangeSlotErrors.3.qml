import QtQuick 2.0

Item {
    property int changeCount: 0

    // invalid property name - we don't allow $
    property bool $nameWithDollarsign: false

    on$NameWithDollarsignChanged: {
        changeCount = changeCount + 4;
    }
}
