import QtQuick 2.0
import "NestedDirOne"
import "NestedDirOne/NestedDirTwo/../../../SpecificComponent"

// NestedDirectoriesTopLevelComponent

Item {
    id: ndtlcId
    property NDComponentOne a: NDComponentOne { }
    property NDComponentOne b: NDComponentOne { }
    property SpecificComponent scOne: SpecificComponent { }
    property SpecificComponent scTwo

    function assignScTwo() {
        // It seems that doing this in onCompleted doesn't work,
        // since that handler will be evaluated after the
        // componentUrlCanonicalization.3.qml onCompleted handler.
        // So, call this function manually....
        var c1 = Qt.createComponent("NestedDirOne/NestedDirTwo/NDComponentTwo.qml");
        var o1 = c1.createObject(ndtlcId);
        scTwo = o1.sc;
    }
}
