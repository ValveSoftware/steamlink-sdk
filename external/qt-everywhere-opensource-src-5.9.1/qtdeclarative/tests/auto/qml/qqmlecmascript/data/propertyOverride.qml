import QtQuick 2.0

Item {
    property bool success: false

    BaseComponent2 {
        id: foo

        // Override the properties of the base component
        property string directProperty: 'hello'
        function getDirectFromExtension() { return directProperty }

        property alias extensionDirectAlias: foo.directProperty

        property Text objectProperty: Text { width: 666 }

        property int optimizedBoundProperty: objectProperty.width
        function getOptimizedBoundFromExtension() { return optimizedBoundProperty }

        property alias extensionOptimizedBoundAlias: foo.optimizedBoundProperty

        property int unoptimizedBoundProperty: if (true) objectProperty.width
        function getUnoptimizedBoundFromExtension() { return unoptimizedBoundProperty }

        property alias extensionUnoptimizedBoundAlias: foo.unoptimizedBoundProperty

        property string extensionDirectPropertyChangedValue: ''
        onDirectPropertyChanged: extensionDirectPropertyChangedValue = directProperty

        property int extensionOptimizedBoundPropertyChangedValue: 0
        onOptimizedBoundPropertyChanged: extensionOptimizedBoundPropertyChangedValue = optimizedBoundProperty

        property int extensionUnoptimizedBoundPropertyChangedValue: 0
        onUnoptimizedBoundPropertyChanged: extensionUnoptimizedBoundPropertyChangedValue = unoptimizedBoundProperty

        function setDirectFromExtension(n) { directProperty = n }
        function setBoundFromExtension(n) { objectProperty.width = n }
    }

    Component.onCompleted: {
        // In the base component, overriding should not affect resolution
        if (foo.getDirectFromBase() != 333) return
        if (foo.getOptimizedBoundFromBase() != 333) return
        if (foo.getUnoptimizedBoundFromBase() != 333) return

        // In the extension component overriding should occur
        if (foo.getDirectFromExtension() != 'hello') return
        if (foo.getOptimizedBoundFromExtension() != 666) return
        if (foo.getUnoptimizedBoundFromExtension() != 666) return

        // External access should yield extension component scoping
        if (foo.directProperty != 'hello') return
        if (foo.optimizedBoundProperty != 666) return
        if (foo.unoptimizedBoundProperty != 666) return

        // Verify alias properties bind to the correct target
        if (foo.baseDirectAlias != 333) return
        if (foo.baseOptimizedBoundAlias != 333) return
        if (foo.baseUnoptimizedBoundAlias != 333) return

        if (foo.extensionDirectAlias != 'hello') return
        if (foo.extensionOptimizedBoundAlias != 666) return
        if (foo.extensionUnoptimizedBoundAlias != 666) return

        foo.baseDirectPropertyChangedValue = 0
        foo.baseOptimizedBoundPropertyChangedValue = 0
        foo.baseUnoptimizedBoundPropertyChangedValue = 0
        foo.extensionDirectPropertyChangedValue = ''
        foo.extensionOptimizedBoundPropertyChangedValue = 0
        foo.extensionUnoptimizedBoundPropertyChangedValue = 0

        // Verify that the correct onChanged signal is emitted
        foo.setDirectFromBase(999)
        if (foo.getDirectFromBase() != 999) return
        if (foo.baseDirectPropertyChangedValue != 999) return
        if (foo.extensionDirectPropertyChangedValue != '') return

        foo.setDirectFromExtension('goodbye')
        if (foo.getDirectFromExtension() != 'goodbye') return
        if (foo.extensionDirectPropertyChangedValue != 'goodbye') return
        if (foo.baseDirectPropertyChangedValue != 999) return

        foo.setBoundFromBase(999)
        if (foo.getOptimizedBoundFromBase() != 999) return
        if (foo.getUnoptimizedBoundFromBase() != 999) return
        if (foo.baseOptimizedBoundPropertyChangedValue != 999) return
        if (foo.baseUnoptimizedBoundPropertyChangedValue != 999) return
        if (foo.extensionOptimizedBoundPropertyChangedValue != 0) return
        if (foo.extensionUnoptimizedBoundPropertyChangedValue != 0) return

        foo.baseOptimizedBoundPropertyChangedValue = 0
        foo.baseUnoptimizedBoundPropertyChangedValue = 0

        foo.setBoundFromExtension(123)
        if (foo.getOptimizedBoundFromExtension() != 123) return
        if (foo.getUnoptimizedBoundFromExtension() != 123) return
        if (foo.extensionOptimizedBoundPropertyChangedValue != 123) return
        if (foo.extensionUnoptimizedBoundPropertyChangedValue != 123) return
        if (foo.baseOptimizedBoundPropertyChangedValue != 0) return
        if (foo.baseUnoptimizedBoundPropertyChangedValue != 0) return

        success = true
    }
}
