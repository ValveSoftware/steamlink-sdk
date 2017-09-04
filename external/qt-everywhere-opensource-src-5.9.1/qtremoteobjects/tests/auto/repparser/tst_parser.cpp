/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "repparser.h"

#include <QTemporaryFile>
#include <QTest>
#include <QTextStream>

Q_DECLARE_METATYPE(ASTProperty::Modifier)
Q_DECLARE_METATYPE(ASTModelRole)

class tst_Parser : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testBasic_data();
    void testBasic();
    void testProperties_data();
    void testProperties();
    void testSlots_data();
    void testSlots();
    void testSignals_data();
    void testSignals();
    void testPods_data();
    void testPods();
    void testEnums_data();
    void testEnums();
    void testModels_data();
    void testModels();
    void testInvalid_data();
    void testInvalid();
};

void tst_Parser::testBasic_data()
{
    QTest::addColumn<QString>("content");

    //Comment out "empty" tests that fail QLALR parser...
    //QTest::newRow("empty") << ""; // empty lines are fine...
    QTest::newRow("preprocessor_line_include") << "#include \"foo\"";
    QTest::newRow("preprocessor_line_include_spaces") << "#  include \"foo\"";
    QTest::newRow("preprocessor_line_ifgroup") << "#if 1\n#include \"foo\n#endif";
    //QTest::newRow("comment") << "//This is a comment";
    QTest::newRow("enum") << "ENUM MyEnum {test}";
}

void tst_Parser::testBasic()
{
    QFETCH(QString, content);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << content << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(parser.parse());
}

void tst_Parser::testProperties_data()
{
    QTest::addColumn<QString>("propertyDeclaration");
    QTest::addColumn<QString>("expectedType");
    QTest::addColumn<QString>("expectedName");
    QTest::addColumn<QString>("expectedDefaultValue");
    QTest::addColumn<ASTProperty::Modifier>("expectedModifier");
    QTest::addColumn<bool>("expectedPersistence");

    QTest::newRow("default") << "PROP(QString foo)" << "QString" << "foo" << QString() << ASTProperty::ReadPush << false;
    QTest::newRow("readonly") << "PROP(QString foo READONLY)" << "QString" << "foo" << QString() << ASTProperty::ReadOnly << false;
    QTest::newRow("constant") << "PROP(QString foo CONSTANT)" << "QString" << "foo" << QString() << ASTProperty::Constant << false;
    QTest::newRow("readwrite") << "PROP(QString foo READWRITE)" << "QString" << "foo" << QString() << ASTProperty::ReadWrite << false;
    QTest::newRow("readpush") << "PROP(QString foo READPUSH)" << "QString" << "foo" << QString() << ASTProperty::ReadPush << false;
    QTest::newRow("persisted") << "PROP(QString foo PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::ReadPush << true;
    QTest::newRow("readonly, persisted") << "PROP(QString foo READONLY, PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::ReadOnly << true;
    QTest::newRow("readwrite, persisted") << "PROP(QString foo READWRITE, PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::ReadWrite << true;
    QTest::newRow("readpush, persisted") << "PROP(QString foo READPUSH, PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::ReadPush << true;
    QTest::newRow("constant,persisted") << "PROP(QString foo CONSTANT, PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::Constant << true;
    QTest::newRow("constant,readonly,persisted") << "PROP(QString foo CONSTANT, READONLY, PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::Constant << true;
    QTest::newRow("readonly,constant,persisted") << "PROP(QString foo READONLY,CONSTANT, PERSISTED)" << "QString" << "foo" << QString() << ASTProperty::Constant << true;
    QTest::newRow("defaultWithValue") << "PROP(int foo=1)" << "int" << "foo" << "1" << ASTProperty::ReadPush << false;
    QTest::newRow("readonlyWithValue") << "PROP(int foo=1 READONLY)" << "int" << "foo" << "1" << ASTProperty::ReadOnly << false;
    QTest::newRow("constantWithValue") << "PROP(int foo=1 CONSTANT)" << "int" << "foo" << "1" << ASTProperty::Constant << false;
    QTest::newRow("defaultWhitespaces") << "PROP(  QString   foo  )" << "QString" << "foo" << QString() << ASTProperty::ReadPush << false;
    QTest::newRow("defaultWhitespacesBeforeParentheses") << "PROP     (  QString   foo  )" << "QString" << "foo" << QString() << ASTProperty::ReadPush << false;
    QTest::newRow("readonlyWhitespaces") << "PROP(  QString   foo   READONLY  )" << "QString" << "foo" << QString() << ASTProperty::ReadOnly << false;
    QTest::newRow("constantWhitespaces") << "PROP(  QString   foo   CONSTANT  )" << "QString" << "foo" << QString() << ASTProperty::Constant << false;
    QTest::newRow("defaultWithValueWhitespaces") << "PROP(  int foo  = 1 )" << "int" << "foo" << "1" << ASTProperty::ReadPush << false;
    QTest::newRow("readonlyWithValueWhitespaces") << "PROP(  int foo = 1 READONLY  )" << "int" << "foo" << "1" << ASTProperty::ReadOnly << false;
    QTest::newRow("constantWithValueWhitespaces") << "PROP(  int foo = 1 CONSTANT )" << "int" << "foo" << "1" << ASTProperty::Constant << false;
    QTest::newRow("templatetype") << "PROP(QVector<int> bar)" << "QVector<int>" << "bar" << QString() << ASTProperty::ReadPush << false;
    QTest::newRow("nested templatetype") << "PROP(QMap<int, QVector<int> > bar)" << "QMap<int, QVector<int> >" << "bar" << QString() << ASTProperty::ReadPush << false;
    QTest::newRow("non-int default value") << "PROP(double foo=1.1 CONSTANT)" << "double" << "foo" << "1.1" << ASTProperty::Constant << false;
}

void tst_Parser::testProperties()
{
    QFETCH(QString, propertyDeclaration);
    QFETCH(QString, expectedType);
    QFETCH(QString, expectedName);
    QFETCH(QString, expectedDefaultValue);
    QFETCH(ASTProperty::Modifier, expectedModifier);
    QFETCH(bool, expectedPersistence);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << "class TestClass" << endl;
    stream << "{" << endl;
    stream << propertyDeclaration << endl;
    stream << "};" << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(parser.parse());

    const AST ast = parser.ast();
    QCOMPARE(ast.classes.count(), 1);

    const ASTClass astClass = ast.classes.first();
    const QVector<ASTProperty> properties = astClass.properties;
    QCOMPARE(properties.count(), 1);

    const ASTProperty property = properties.first();
    QCOMPARE(property.type, expectedType);
    QCOMPARE(property.name, expectedName);
    QCOMPARE(property.defaultValue, expectedDefaultValue);
    QCOMPARE(property.modifier, expectedModifier);
    QCOMPARE(property.persisted, expectedPersistence);
}

void tst_Parser::testSlots_data()
{
    QTest::addColumn<QString>("slotDeclaration");
    QTest::addColumn<QString>("expectedSlot");
    QTest::addColumn<bool>("voidWarning");
    QTest::newRow("slotwithoutspacebeforeparentheses") << "SLOT(test())" << "void test()" << true;
    QTest::newRow("slotwithspacebeforeparentheses") << "SLOT (test())" << "void test()" << true;
    QTest::newRow("slotwitharguments") << "SLOT(void test(QString value, int number))" << "void test(QString value, int number)" << false;
    QTest::newRow("slotwithunnamedarguments") << "SLOT(void test(QString, int))" << "void test(QString __repc_variable_1, int __repc_variable_2)" << false;
    QTest::newRow("slotwithspaces") << "SLOT(   void test  (QString value, int number)  )" << "void test(QString value, int number)" << false;
    QTest::newRow("slotwithtemplates") << "SLOT(test(QMap<QString,int> foo))" << "void test(QMap<QString,int> foo)" << true;
    QTest::newRow("slotwithmultitemplates") << "SLOT(test(QMap<QString,int> foo, QMap<QString,int> bla))" << "void test(QMap<QString,int> foo, QMap<QString,int> bla)" << true;
    QTest::newRow("slotwithtemplatetemplates") << "SLOT(test(QMap<QList<QString>,int> foo))" << "void test(QMap<QList<QString>,int> foo)" << true;
    QTest::newRow("slotwithtemplateswithspace") << "SLOT ( test (QMap<QString , int>  foo ) )" << "void test(QMap<QString , int> foo)" << true;
    QTest::newRow("slotWithConstRefArgument") << "SLOT (test(const QString &val))" << "void test(const QString & val)" << true;
    QTest::newRow("slotWithRefArgument") << "SLOT (test(QString &val))" << "void test(QString & val)" << true;
    QTest::newRow("slotwithtemplatetemplatesAndConstRef") << "SLOT(test(const QMap<QList<QString>,int> &foo))" << "void test(const QMap<QList<QString>,int> & foo)" << true;
    QTest::newRow("slotWithConstRefArgumentAndWithout") << "SLOT (test(const QString &val, int value))" << "void test(const QString & val, int value)" << true;
}

void tst_Parser::testSlots()
{
    QFETCH(QString, slotDeclaration);
    QFETCH(QString, expectedSlot);
    QFETCH(bool, voidWarning);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << "class TestClass" << endl;
    stream << "{" << endl;
    stream << slotDeclaration << endl;
    stream << "};" << endl;
    file.seek(0);

    if (voidWarning)
        QTest::ignoreMessage(QtWarningMsg, "[repc] - Adding 'void' for unspecified return type on test");
    RepParser parser(file);
    QVERIFY(parser.parse());

    const AST ast = parser.ast();
    QCOMPARE(ast.classes.count(), 1);

    const ASTClass astClass = ast.classes.first();
    const QVector<ASTFunction> slotsList = astClass.slotsList;
    QCOMPARE(slotsList.count(), 1);
    ASTFunction slot = slotsList.first();
    QCOMPARE(QString("%1 %2(%3)").arg(slot.returnType).arg(slot.name).arg(slot.paramsAsString()), expectedSlot);
}

void tst_Parser::testSignals_data()
{
    QTest::addColumn<QString>("signalDeclaration");
    QTest::addColumn<QString>("expectedSignal");
    QTest::newRow("signalwithoutspacebeforeparentheses") << "SIGNAL(test())" << "test()";
    QTest::newRow("signalwithspacebeforeparentheses") << "SIGNAL (test())" << "test()";
    QTest::newRow("signalwitharguments") << "SIGNAL(test(QString value, int value))" << "test(QString value, int value)";
    QTest::newRow("signalwithtemplates") << "SIGNAL(test(QMap<QString,int> foo))" << "test(QMap<QString,int> foo)";
    QTest::newRow("signalwithtemplateswithspace") << "SIGNAL ( test (QMap<QString , int>  foo ) )" << "test(QMap<QString , int> foo)";
    QTest::newRow("signalWithConstRefArgument") << "SIGNAL (test(const QString &val))" << "test(const QString & val)";
    QTest::newRow("signalWithRefArgument") << "SIGNAL (test(QString &val))" << "test(QString & val)";
}

void tst_Parser::testSignals()
{
    QFETCH(QString, signalDeclaration);
    QFETCH(QString, expectedSignal);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << "class TestClass" << endl;
    stream << "{" << endl;
    stream << signalDeclaration << endl;
    stream << "};" << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(parser.parse());

    const AST ast = parser.ast();
    QCOMPARE(ast.classes.count(), 1);

    const ASTClass astClass = ast.classes.first();
    const QVector<ASTFunction> signalsList = astClass.signalsList;
    ASTFunction signal = signalsList.first();
    QCOMPARE(QString("%1(%2)").arg(signal.name).arg(signal.paramsAsString()), expectedSignal);
}

void tst_Parser::testPods_data()
{
    QTest::addColumn<QString>("podsdeclaration");
    QTest::addColumn<QString>("expectedtypes");
    QTest::addColumn<QString>("expectedvariables");

    //Variable/Type separate by ";"
    QTest::newRow("one pod") << "POD preset(int presetNumber)" << "int" << "presetNumber";
    QTest::newRow("two pod") << "POD preset(int presetNumber, double foo)" << "int;double" << "presetNumber;foo";
    QTest::newRow("two pod with space") << "POD preset ( int presetNumber , double foo ) " << "int;double" << "presetNumber;foo";
    //Template
    QTest::newRow("pod template") << "POD preset(QMap<QString,int> foo) " << "QMap<QString,int>" << "foo";
    QTest::newRow("pod template (QList)") << "POD preset(QList<QString> foo) " << "QList<QString>" << "foo";
    QTest::newRow("two pod template") << "POD preset(QMap<QString,int> foo, QMap<double,int> bla) " << "QMap<QString,int>;QMap<double,int>" << "foo;bla";
    QTest::newRow("two pod template with space") << "POD preset( QMap<QString  ,  int >  foo ,   QMap<  double , int > bla ) " << "QMap<QString  ,  int >;QMap<  double , int >" << "foo;bla";

}

void tst_Parser::testPods()
{
    QFETCH(QString, podsdeclaration);
    QFETCH(QString, expectedtypes);
    QFETCH(QString, expectedvariables);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << podsdeclaration << endl;
    stream << "class TestClass" << endl;
    stream << "{" << endl;
    stream << "};" << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(parser.parse());

    const AST ast = parser.ast();
    QCOMPARE(ast.classes.count(), 1);

    QCOMPARE(ast.pods.count(), 1);
    const POD pods = ast.pods.first();
    const QVector<PODAttribute> podsList = pods.attributes;
    const QStringList typeList = expectedtypes.split(QLatin1Char(';'));
    const QStringList variableList = expectedvariables.split(QLatin1Char(';'));
    QVERIFY(typeList.count() == variableList.count());
    QVERIFY(podsList.count() == variableList.count());
    for (int i=0; i < podsList.count(); ++i) {
        QCOMPARE(podsList.at(i).name, variableList.at(i));
        QCOMPARE(podsList.at(i).type, typeList.at(i));
    }
}

void tst_Parser::testEnums_data()
{
    QTest::addColumn<QString>("enumdeclaration");
    QTest::addColumn<QString>("expectednames");
    QTest::addColumn<QList<int> >("expectedvalues");
    QTest::addColumn<int>("expectedmax");
    QTest::addColumn<bool>("expectedsigned");

    //Separate by ";"
    QTest::newRow("one enum val") << "ENUM preset {presetNumber}" << "presetNumber" << (QList<int>() << 0) << 0 << false;
    QTest::newRow("two enum val") << "ENUM preset {presetNumber, foo}" << "presetNumber;foo" << (QList<int>() << 0 << 1) << 1 << false;
    QTest::newRow("two enum val -1 2nd") << "ENUM preset {presetNumber, foo = -1}" << "presetNumber;foo" << (QList<int>() << 0 << -1) << 1 << true;
    QTest::newRow("two enum val -1 1st") << "ENUM preset {presetNumber=-1, foo}" << "presetNumber;foo" << (QList<int>() << -1 << 0) << 1 << true;
    QTest::newRow("two enum val hex") << "ENUM preset {presetNumber=0xf, foo}" << "presetNumber;foo" << (QList<int>() << 15 << 16) << 16 << false;
    QTest::newRow("two enum val hex") << "ENUM preset {presetNumber=0xff, foo}" << "presetNumber;foo" << (QList<int>() << 255 << 256) << 256 << false;
    QTest::newRow("two enum val with space") << "ENUM preset { presetNumber ,  foo } " << "presetNumber;foo" << (QList<int>() << 0 << 1) << 1 << false;
    QTest::newRow("set values") << "ENUM preset { val1=1 , val3=3, val5=5 } " << "val1;val3;val5" << (QList<int>() << 1 << 3 << 5) << 5 << false;
}

void tst_Parser::testEnums()
{
    QFETCH(QString, enumdeclaration);
    QFETCH(QString, expectednames);
    QFETCH(QList<int>, expectedvalues);
    QFETCH(int, expectedmax);
    QFETCH(bool, expectedsigned);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << enumdeclaration << endl;
    stream << "class TestClass" << endl;
    stream << "{" << endl;
    stream << "};" << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(parser.parse());

    const AST ast = parser.ast();
    QCOMPARE(ast.classes.count(), 1);

    QCOMPARE(ast.enums.count(), 1);
    const ASTEnum enums = ast.enums.first();
    const QVector<ASTEnumParam> paramList = enums.params;
    const QStringList nameList = expectednames.split(QLatin1Char(';'));
    QVERIFY(nameList.count() == expectedvalues.count());
    QVERIFY(paramList.count() == expectedvalues.count());
    for (int i=0; i < paramList.count(); ++i) {
        QCOMPARE(paramList.at(i).name, nameList.at(i));
        QCOMPARE(paramList.at(i).value, expectedvalues.at(i));
    }
    QCOMPARE(enums.max, expectedmax);
    QCOMPARE(enums.isSigned, expectedsigned);
}

void tst_Parser::testModels_data()
{
    QTest::addColumn<QString>("modelDeclaration");
    QTest::addColumn<QString>("expectedModel");
    QTest::addColumn<QVector<ASTModelRole>>("expectedRoles");
    QTest::newRow("basicmodel") << "MODEL test(display)" << "test" << QVector<ASTModelRole>({{"display"}});
    QTest::newRow("basicmodelsemicolon") << "MODEL test(display);" << "test" << QVector<ASTModelRole>({{"display"}});
}

void tst_Parser::testModels()
{
    QFETCH(QString, modelDeclaration);
    QFETCH(QString, expectedModel);
    QFETCH(QVector<ASTModelRole>, expectedRoles);

    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << "class TestClass" << endl;
    stream << "{" << endl;
    stream << modelDeclaration << endl;
    stream << "};" << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(parser.parse());

    const AST ast = parser.ast();
    QCOMPARE(ast.classes.count(), 1);

    const ASTClass astClass = ast.classes.first();
    ASTModel model = astClass.models.first();
    QCOMPARE(model.name, expectedModel);
    int i = 0;
    for (auto role : model.roles) {
        QCOMPARE(role.name, expectedRoles.at(i).name);
        i++;
    }
}

void tst_Parser::testInvalid_data()
{
    QTest::addColumn<QString>("content");
    QTest::addColumn<QString>("warning");

    QTest::newRow("pod_invalid") << "POD (int foo)" << ".?Unknown token encountered";
    QTest::newRow("pod_unbalancedparens") << "POD foo(int foo" << ".?Unknown token encountered";
    QTest::newRow("pod_inclass") << "class Foo\n{\nPOD foo(int)\n}" << ".?POD: Can only be used in global scope";
    QTest::newRow("class_noidentifier") << "class\n{\n}" << ".?Unknown token encountered";
    QTest::newRow("class_nested") << "class Foo\n{\nclass Bar\n}" << ".?class: Cannot be nested";
    QTest::newRow("prop_outsideclass") << "PROP(int foo)" << ".?PROP: Can only be used in class scope";
    QTest::newRow("prop_toomanyargs") << "class Foo\n{\nPROP(int int foo)\n}" << ".?Invalid property declaration: flag foo is unknown";
    QTest::newRow("prop_toomanymodifiers") << "class Foo\n{\nPROP(int foo READWRITE, READONLY)\n}" << ".?Invalid property declaration: combination not allowed .READWRITE, READONLY.";
    QTest::newRow("prop_noargs") << "class Foo\n{\nPROP()\n}" << ".?Unknown token encountered";
    QTest::newRow("prop_unbalancedparens") << "class Foo\n{\nPROP(int foo\n}" << ".?Unknown token encountered";
    QTest::newRow("signal_outsideclass") << "SIGNAL(foo())" << ".?SIGNAL: Can only be used in class scope";
    QTest::newRow("signal_noargs") << "class Foo\n{\nSIGNAL()\n}" << ".?Unknown token encountered";
    QTest::newRow("slot_outsideclass") << "SLOT(void foo())" << ".?SLOT: Can only be used in class scope";
    QTest::newRow("slot_noargs") << "class Foo\n{\nSLOT()\n}" << ".?Unknown token encountered";
    QTest::newRow("model_outsideclass") << "MODEL foo" << ".?Unknown token encountered";
    QTest::newRow("preprecessor_line_inclass") << "class Foo\n{\n#define foo\n}" << ".?Unknown token encountered";
}

void tst_Parser::testInvalid()
{
    QFETCH(QString, content);
    QFETCH(QString, warning);

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(warning));
    QTemporaryFile file;
    file.open();
    QTextStream stream(&file);
    stream << content << endl;
    file.seek(0);

    RepParser parser(file);
    QVERIFY(!parser.parse());
}

QTEST_APPLESS_MAIN(tst_Parser)

#include "tst_parser.moc"
