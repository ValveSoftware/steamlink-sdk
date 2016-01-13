import QtQuick 2.0
import "SpecificComponent"
import "OtherComponent"
import "NestedDirectories"

Item {
    id: root

    property SpecificComponent scOne
    property SpecificComponent scTwo
    property SpecificComponent scThree
    property SpecificComponent scFour
    property SpecificComponent scFive
    property SpecificComponent scSix
    property SpecificComponent scSeven
    property SpecificComponent scEight

    property OtherComponent ocOne: OtherComponent { }
    property NDTLC ndtlc: NDTLC { }

    property bool success: false

    Component.onCompleted: {
        var c1 = Qt.createComponent("./SpecificComponent/SpecificComponent.qml");
        var o1 = c1.createObject(root);
        scOne = o1;
        scTwo = ocOne.sc;
        scThree = ndtlc.a.sc;
        scFour = ndtlc.b.sc;
        scFive = ndtlc.a.b.sc;
        scSix = ndtlc.b.b.sc;
        scSeven = ndtlc.scOne;
        ndtlc.assignScTwo(); // XXX should be able to do this in NDTLC.onCompleted handler?!
        scEight = ndtlc.scTwo;

        // in our case, the type string should be:
        // SpecificComponent_QMLTYPE_0
        var t1 = scOne.toString().substr(0, scOne.toString().indexOf('('));
        var t2 = scTwo.toString().substr(0, scTwo.toString().indexOf('('));
        var t3 = scThree.toString().substr(0, scThree.toString().indexOf('('));
        var t4 = scFour.toString().substr(0, scFour.toString().indexOf('('));
        var t5 = scFive.toString().substr(0, scFive.toString().indexOf('('));
        var t6 = scSix.toString().substr(0, scSix.toString().indexOf('('));
        var t7 = scSeven.toString().substr(0, scSeven.toString().indexOf('('));
        var t8 = scEight.toString().substr(0, scEight.toString().indexOf('('));

        if (t1 == t2 && t2 == t3 && t3 == t4 && t4 == t5 && t5 == t6 && t6 == t7 && t7 == t8) {
            success = true;
        } else {
            success = false;
        }
    }
}
