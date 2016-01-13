import QtQuick 2.0

Item {
    id: base

    property int directProperty: 333
    function getDirectFromBase() { return directProperty }

    property alias baseDirectAlias: base.directProperty

    property Item objectProperty: Item { width: 333 }

    property int optimizedBoundProperty: objectProperty.width
    function getOptimizedBoundFromBase() { return optimizedBoundProperty }

    property alias baseOptimizedBoundAlias: base.optimizedBoundProperty

    property int unoptimizedBoundProperty: if (true) objectProperty.width
    function getUnoptimizedBoundFromBase() { return unoptimizedBoundProperty }

    property alias baseUnoptimizedBoundAlias: base.unoptimizedBoundProperty

    property int baseDirectPropertyChangedValue: 0
    onDirectPropertyChanged: baseDirectPropertyChangedValue = directProperty

    property int baseOptimizedBoundPropertyChangedValue: 0
    onOptimizedBoundPropertyChanged: baseOptimizedBoundPropertyChangedValue = optimizedBoundProperty

    property int baseUnoptimizedBoundPropertyChangedValue: 0
    onUnoptimizedBoundPropertyChanged: baseUnoptimizedBoundPropertyChangedValue = unoptimizedBoundProperty

    function setDirectFromBase(n) { directProperty = n }
    function setBoundFromBase(n) { objectProperty.width = n }
}
