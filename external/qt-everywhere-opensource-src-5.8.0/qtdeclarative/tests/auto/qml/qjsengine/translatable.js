qsTr("One");
qsTranslate("FooContext", "Two");

var greeting_strings = [
    QT_TR_NOOP("Hello"),
    QT_TRANSLATE_NOOP("FooContext", "Goodbye")
];

qsTr("One", "not the same one");

qsTr("%n message(s) saved", "", 10);
qsTranslate("FooContext", "%n fooish bar(s) found", "", 10);
