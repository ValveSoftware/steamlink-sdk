// No context specified, default should be used.
qsTr("One");
QT_TR_NOOP("Two");

// TRANSLATOR Foo
qsTr("Three");
QT_TR_NOOP("Four");

// TRANSLATOR Bar
qsTr("Five");

/*
    TRANSLATOR Baz
    This is a comment to the translator.
*/
QT_TR_NOOP("Six");

// TRANSLATOR Foo.Bar
qsTr("Seven");

/* TRANSLATOR Bar::Baz */
QT_TR_NOOP("Eight");

// qsTranslate() context is not affected.
qsTranslate("Foo", "Nine");

// Empty context.
// TRANSLATOR
qsTr("Ten");
