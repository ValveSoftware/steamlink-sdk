QT += core-private xmlpatterns xmlpatterns-private
QT -= gui

QMAKE_RPATHLINKDIR *= $$QT.gui.libs

XMLPATTERNS_SDK = QtXmlPatternsSDK
if(!debug_and_release|build_pass):CONFIG(debug, debug|release) {
    win32: XMLPATTERNS_SDK = $${XMLPATTERNS_SDK}d
    else:  XMLPATTERNS_SDK = $${XMLPATTERNS_SDK}_debug
}
