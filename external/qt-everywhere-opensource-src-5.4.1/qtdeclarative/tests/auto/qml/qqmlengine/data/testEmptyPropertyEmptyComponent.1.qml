import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testEmptyPropertyEmptyComponent.1.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('EmptyPropertyEmptyComponent.qml')) return reportError('Property component already loaded')
        if (componentCache.isTypeLoaded('EmptyComponent.qml')) return reportError('Empty component already loaded')

        var comp = Qt.createComponent('EmptyPropertyEmptyComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testEmptyPropertyEmptyComponent.1.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('EmptyPropertyEmptyComponent.qml')) return reportError('Property component not loaded')
        if (!componentCache.isTypeLoaded('EmptyComponent.qml')) return reportError('Empty component not loaded')

        var obj = comp.createObject()
        if (!obj) return reportError('Invalid object')
        if (obj.x == undefined) return reportError('Invalid object 2')
        if (obj.p == undefined) return reportError('Invalid object 3')
        if (obj.p.x == undefined) return reportError('Invalid object 4')

        obj.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testEmptyPropertyEmptyComponent.1.qml')) return reportError('Test component not loaded 3')
        if (!componentCache.isTypeLoaded('EmptyPropertyEmptyComponent.qml')) return reportError('Property component already unloaded')
        if (!componentCache.isTypeLoaded('EmptyComponent.qml')) return reportError('Empty component already unloaded')

        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testEmptyPropertyEmptyComponent.1.qml')) return reportError('Test component not loaded 4')
        if (componentCache.isTypeLoaded('EmptyPropertyEmptyComponent.qml')) return reportError('Property component not unloaded')
        if (componentCache.isTypeLoaded('EmptyComponent.qml')) return reportError('Empty component not unloaded')

        success = true
    }
}
