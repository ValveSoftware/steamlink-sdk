import QtQuick 2.0

Item {
    property bool success: false

    function reportError(s) { console.warn(s) }

    Component.onCompleted: {
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testScriptComponent.qml')) return reportError('Test component not loaded')
        if (componentCache.isTypeLoaded('ScriptComponent.qml')) return reportError('Script component already loaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component already loaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component already loaded')
        if (componentCache.isScriptLoaded('script.js')) return reportError('Script file already loaded')

        var comp = Qt.createComponent('ScriptComponent.qml')
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testScriptComponent.qml')) return reportError('Test component not loaded 2')
        if (!componentCache.isTypeLoaded('ScriptComponent.qml')) return reportError('Script component not loaded')
        if (!componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not loaded')
        if (!componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not loaded')
        if (!componentCache.isScriptLoaded('script.js')) return reportError('Script file not loaded')

        var obj = comp.createObject()
        if (!obj) return reportError('Invalid object')
        if (obj.foo != 'bar') return reportError('Invalid object 2')
        if (obj.bar != 'baz') return reportError('Invalid object 3')
        if (obj.getSomething() != 'https://example.org/') return reportError('Invalid object 4')

        obj.destroy()
        comp.destroy()
        componentCache.trim()
        if (!componentCache.isTypeLoaded('testScriptComponent.qml')) return reportError('Test component not loaded 3')
        if (componentCache.isTypeLoaded('ScriptComponent.qml')) return reportError('Script component not unloaded')
        if (componentCache.isTypeLoaded('VMEExtendVMEComponent.qml')) return reportError('Extend component not unloaded')
        if (componentCache.isTypeLoaded('VMEComponent.qml')) return reportError('VME component not unloaded')

        // Script unloading is not currently implemented
        //if (componentCache.isScriptLoaded('script.js')) return reportError('Script file already loaded')

        success = true
    }
}
