import Qt.test 1.0
import QtQuick 2.0

MyTypeObject{
    id: obj
    objectName: "obj"

    function url()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml');
    }

    function urlMode()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', Component.PreferSynchronous);
    }

    function urlParent()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', obj);
    }

    function urlNullParent()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', null);
    }

    function urlModeParent()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', Component.PreferSynchronous, obj);
    }

    function urlModeNullParent()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', Component.PreferSynchronous, null);
    }

    function invalidSecondArg()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', 'Bad argument');
    }

    function invalidThirdArg()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', Component.PreferSynchronous, 'Bad argument');
    }

    function invalidMode()
    {
        obj.componentProperty = Qt.createComponent('dynamicCreation.helper.qml', -666);
    }
}
