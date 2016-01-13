TEMPLATE = subdirs

SUBDIRS += \
           parserstress \
           qdeclarativecomponent \
           qdeclarativecontext \
           qdeclarativeengine \
           qdeclarativeerror \
           qdeclarativefolderlistmodel \
           qdeclarativeinfo \
           qdeclarativelayoutitem \
           qdeclarativelistreference \
           qdeclarativemetatype \
           qdeclarativemoduleplugin \
           qdeclarativeparticles \
           qdeclarativepixmapcache \
           qdeclarativeqt \
           qdeclarativeview \
           qdeclarativeviewer \
           qdeclarativexmlhttprequest \
           qmlvisual \
           moduleqt47

contains(QT_CONFIG, private_tests) {
    SUBDIRS += \
           examples \
           qdeclarativeanchors \
           qdeclarativeanimatedimage \
           qdeclarativeanimations \
           qdeclarativeapplication \
           qdeclarativebehaviors \
           qdeclarativebinding \
           qdeclarativeborderimage \
           qdeclarativeconnection \
           qdeclarativedebug \
           qdeclarativedebugclient \
           qdeclarativedebugservice \
           qdeclarativedebugjs \
           qdeclarativedebugobservermode \
           qdeclarativedom \
           qdeclarativeecmascript \
           qdeclarativeflickable \
           qdeclarativeflipable \
           qdeclarativefocusscope \
           qdeclarativefontloader \
           qdeclarativegridview \
           qdeclarativeimage \
           qdeclarativeimageprovider \
           qdeclarativeinstruction \
           qdeclarativeitem \
           qdeclarativelanguage \
           qdeclarativelistmodel \
           qdeclarativelistview \
           qdeclarativeloader \
           qdeclarativemousearea \
           qdeclarativepathview \
           qdeclarativepincharea \
           qdeclarativepositioners \
           qdeclarativeproperty \
           qdeclarativepropertymap \
           qdeclarativerepeater \
           qdeclarativesmoothedanimation \
           qdeclarativespringanimation \
           qdeclarativestyledtext \
           qdeclarativestates \
           qdeclarativesystempalette \
           qdeclarativetext \
           qdeclarativetextedit \
           qdeclarativetextinput \
           qdeclarativetimer \
           qdeclarativevaluetypes \
           qdeclarativevisualdatamodel \
           qdeclarativeworkerscript \
           qdeclarativexmllistmodel \
           qpacketprotocol

    # This test requires the xmlpatterns module
    !qtHaveModule(xmlpatterns): SUBDIRS -= qdeclarativexmllistmodel
}

qtHaveModule(opengl): SUBDIRS += qmlshadersplugin

qtHaveModule(webkit): SUBDIRS += qdeclarativewebview

# Tests which should run in Pulse
PULSE_TESTS = $$SUBDIRS
