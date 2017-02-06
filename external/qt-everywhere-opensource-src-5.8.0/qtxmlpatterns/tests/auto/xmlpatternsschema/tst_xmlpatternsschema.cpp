/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite module of the Qt Toolkit.
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

#include <QtTest/QtTest>

#include <private/qxsdstatemachine_p.h>
#include <private/qxsdschematoken_p.h>

using namespace QPatternist;

class tst_XMLPatternsSchema : public QObject
{
    Q_OBJECT

    public slots:
        void init();
        void cleanup();

    private slots:
        void stateMachineTest1();
        void stateMachineTest2();
        void stateMachineTest3();
        void stateMachineTest4();
        void stateMachineTest5();
        void stateMachineTest6();
        void stateMachineTest7();
        void stateMachineTest8();
        void stateMachineTest9();
        void stateMachineTest10();
        void stateMachineTest11();
        void stateMachineTest12();
        void stateMachineTest13();
        void stateMachineTest14();
        void stateMachineTest15();
        void stateMachineTest16();
        void stateMachineTest17();
        void stateMachineTest18();
        void stateMachineTest19();

    private:
        bool runTest(const QVector<XsdSchemaToken::NodeName> &list, XsdStateMachine<XsdSchemaToken::NodeName> &machine);
};

void tst_XMLPatternsSchema::init()
{
}

void tst_XMLPatternsSchema::cleanup()
{
}

bool tst_XMLPatternsSchema::runTest(const QVector<XsdSchemaToken::NodeName> &list, XsdStateMachine<XsdSchemaToken::NodeName> &machine)
{
    machine.reset();
    for (int i = 0; i < list.count(); ++i) {
        if (!machine.proceed(list.at(i)))
            return false;
    }
    if (!machine.inEndState())
        return false;

    return true;
}

void tst_XMLPatternsSchema::stateMachineTest1()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, simpleType?) : attribute
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);

    QVERIFY(machine.inEndState() == true);
    QVERIFY(machine.proceed(XsdSchemaToken::Annotation) == true);
    QVERIFY(machine.inEndState() == true);
    QVERIFY(machine.proceed(XsdSchemaToken::SimpleType) == true);
    QVERIFY(machine.inEndState() == true);
    QVERIFY(machine.proceed(XsdSchemaToken::SimpleType) == false);
    QVERIFY(machine.proceed(XsdSchemaToken::Annotation) == false);
    machine.reset();
    QVERIFY(machine.proceed(XsdSchemaToken::SimpleType) == true);
    QVERIFY(machine.inEndState() == true);
    QVERIFY(machine.proceed(XsdSchemaToken::Annotation) == false);
}

void tst_XMLPatternsSchema::stateMachineTest2()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, ((simpleType | complexType)?, (unique | key | keyref)*)) : element
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s5 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s6 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(startState, XsdSchemaToken::ComplexType, s3);
    machine.addTransition(startState, XsdSchemaToken::Unique, s4);
    machine.addTransition(startState, XsdSchemaToken::Key, s5);
    machine.addTransition(startState, XsdSchemaToken::Keyref, s6);

    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s1, XsdSchemaToken::ComplexType, s3);
    machine.addTransition(s1, XsdSchemaToken::Unique, s4);
    machine.addTransition(s1, XsdSchemaToken::Key, s5);
    machine.addTransition(s1, XsdSchemaToken::Keyref, s6);

    machine.addTransition(s2, XsdSchemaToken::Unique, s4);
    machine.addTransition(s2, XsdSchemaToken::Key, s5);
    machine.addTransition(s2, XsdSchemaToken::Keyref, s6);

    machine.addTransition(s3, XsdSchemaToken::Unique, s4);
    machine.addTransition(s3, XsdSchemaToken::Key, s5);
    machine.addTransition(s3, XsdSchemaToken::Keyref, s6);

    machine.addTransition(s4, XsdSchemaToken::Unique, s4);
    machine.addTransition(s4, XsdSchemaToken::Key, s5);
    machine.addTransition(s4, XsdSchemaToken::Keyref, s6);

    machine.addTransition(s5, XsdSchemaToken::Unique, s4);
    machine.addTransition(s5, XsdSchemaToken::Key, s5);
    machine.addTransition(s5, XsdSchemaToken::Keyref, s6);

    machine.addTransition(s6, XsdSchemaToken::Unique, s4);
    machine.addTransition(s6, XsdSchemaToken::Key, s5);
    machine.addTransition(s6, XsdSchemaToken::Keyref, s6);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3;

    data1 << XsdSchemaToken::Annotation
          << XsdSchemaToken::SimpleType
          << XsdSchemaToken::Unique
          << XsdSchemaToken::Keyref;

    data2 << XsdSchemaToken::Annotation
          << XsdSchemaToken::SimpleType
          << XsdSchemaToken::Annotation
          << XsdSchemaToken::Keyref;

    data3 << XsdSchemaToken::Annotation
          << XsdSchemaToken::SimpleType
          << XsdSchemaToken::SimpleType;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == false);
    QVERIFY(runTest(data3, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest3()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (simpleContent | complexContent | ((group | all | choice | sequence)?, ((attribute | attributeGroup)*, anyAttribute?)))) : complexType
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s5 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s6 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s7 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s8 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s9 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s10 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleContent, s2);
    machine.addTransition(startState, XsdSchemaToken::ComplexContent, s3);
    machine.addTransition(startState, XsdSchemaToken::Group, s4);
    machine.addTransition(startState, XsdSchemaToken::All, s5);
    machine.addTransition(startState, XsdSchemaToken::Choice, s6);
    machine.addTransition(startState, XsdSchemaToken::Sequence, s7);
    machine.addTransition(startState, XsdSchemaToken::Attribute, s8);
    machine.addTransition(startState, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(startState, XsdSchemaToken::AnyAttribute, s10);

    machine.addTransition(s1, XsdSchemaToken::SimpleContent, s2);
    machine.addTransition(s1, XsdSchemaToken::ComplexContent, s3);
    machine.addTransition(s1, XsdSchemaToken::Group, s4);
    machine.addTransition(s1, XsdSchemaToken::All, s5);
    machine.addTransition(s1, XsdSchemaToken::Choice, s6);
    machine.addTransition(s1, XsdSchemaToken::Sequence, s7);
    machine.addTransition(s1, XsdSchemaToken::Attribute, s8);
    machine.addTransition(s1, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(s1, XsdSchemaToken::AnyAttribute, s10);

    machine.addTransition(s4, XsdSchemaToken::Attribute, s8);
    machine.addTransition(s4, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(s4, XsdSchemaToken::AnyAttribute, s10);

    machine.addTransition(s5, XsdSchemaToken::Attribute, s8);
    machine.addTransition(s5, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(s5, XsdSchemaToken::AnyAttribute, s10);

    machine.addTransition(s6, XsdSchemaToken::Attribute, s8);
    machine.addTransition(s6, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(s6, XsdSchemaToken::AnyAttribute, s10);

    machine.addTransition(s7, XsdSchemaToken::Attribute, s8);
    machine.addTransition(s7, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(s7, XsdSchemaToken::AnyAttribute, s10);

    machine.addTransition(s8, XsdSchemaToken::Attribute, s8);
    machine.addTransition(s8, XsdSchemaToken::AttributeGroup, s9);
    machine.addTransition(s8, XsdSchemaToken::AnyAttribute, s10);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4;

    data1 << XsdSchemaToken::Annotation
          << XsdSchemaToken::SimpleContent;

    data2 << XsdSchemaToken::Group
          << XsdSchemaToken::Attribute
          << XsdSchemaToken::Attribute
          << XsdSchemaToken::AnyAttribute;

    data3 << XsdSchemaToken::Group
          << XsdSchemaToken::Choice
          << XsdSchemaToken::Attribute
          << XsdSchemaToken::AnyAttribute;

    data4 << XsdSchemaToken::Annotation
          << XsdSchemaToken::Sequence
          << XsdSchemaToken::AnyAttribute;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == false);
    QVERIFY(runTest(data4, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest4()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, ((attribute | attributeGroup)*, anyAttribute?)) : named attribute group/simple content extension/
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Attribute, s2);
    machine.addTransition(startState, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(startState, XsdSchemaToken::AnyAttribute, s4);

    machine.addTransition(s1, XsdSchemaToken::Attribute, s2);
    machine.addTransition(s1, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(s1, XsdSchemaToken::AnyAttribute, s4);

    machine.addTransition(s2, XsdSchemaToken::Attribute, s2);
    machine.addTransition(s2, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(s2, XsdSchemaToken::AnyAttribute, s4);

    machine.addTransition(s3, XsdSchemaToken::Attribute, s2);
    machine.addTransition(s3, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(s3, XsdSchemaToken::AnyAttribute, s4);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4;

    data1 << XsdSchemaToken::Annotation;

    data2 << XsdSchemaToken::Attribute
          << XsdSchemaToken::Attribute
          << XsdSchemaToken::Attribute
          << XsdSchemaToken::AnyAttribute;

    data3 << XsdSchemaToken::Group
          << XsdSchemaToken::Attribute
          << XsdSchemaToken::AnyAttribute;

    data4 << XsdSchemaToken::Attribute
          << XsdSchemaToken::AnyAttribute
          << XsdSchemaToken::Attribute;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == false);
    QVERIFY(runTest(data4, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest5()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (all | choice | sequence)?) : group
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::All, s2);
    machine.addTransition(startState, XsdSchemaToken::Choice, s3);
    machine.addTransition(startState, XsdSchemaToken::Sequence, s4);

    machine.addTransition(s1, XsdSchemaToken::All, s2);
    machine.addTransition(s1, XsdSchemaToken::Choice, s3);
    machine.addTransition(s1, XsdSchemaToken::Sequence, s4);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6, data7, data8, data9;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::All;
    data3 << XsdSchemaToken::Annotation << XsdSchemaToken::Choice;
    data4 << XsdSchemaToken::Annotation << XsdSchemaToken::Sequence;
    data5 << XsdSchemaToken::All;
    data6 << XsdSchemaToken::Choice;
    data7 << XsdSchemaToken::Sequence;
    data8 << XsdSchemaToken::Sequence << XsdSchemaToken::All;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == true);
    QVERIFY(runTest(data6, machine) == true);
    QVERIFY(runTest(data7, machine) == true);
    QVERIFY(runTest(data8, machine) == false);
    QVERIFY(runTest(data9, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest6()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, element*) : all
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Element, s2);

    machine.addTransition(s1, XsdSchemaToken::Element, s2);

    machine.addTransition(s2, XsdSchemaToken::Element, s2);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6, data7, data8, data9;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Element;
    data3 << XsdSchemaToken::Element;
    data4 << XsdSchemaToken::Element << XsdSchemaToken::Sequence;
    data5 << XsdSchemaToken::Annotation << XsdSchemaToken::Element << XsdSchemaToken::Annotation;
    data6 << XsdSchemaToken::Annotation << XsdSchemaToken::Annotation << XsdSchemaToken::Element;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == false);
    QVERIFY(runTest(data5, machine) == false);
    QVERIFY(runTest(data6, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest7()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (element | group | choice | sequence | any)*) : choice sequence
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s5 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s6 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Element, s2);
    machine.addTransition(startState, XsdSchemaToken::Group, s3);
    machine.addTransition(startState, XsdSchemaToken::Choice, s4);
    machine.addTransition(startState, XsdSchemaToken::Sequence, s5);
    machine.addTransition(startState, XsdSchemaToken::Any, s6);

    machine.addTransition(s1, XsdSchemaToken::Element, s2);
    machine.addTransition(s1, XsdSchemaToken::Group, s3);
    machine.addTransition(s1, XsdSchemaToken::Choice, s4);
    machine.addTransition(s1, XsdSchemaToken::Sequence, s5);
    machine.addTransition(s1, XsdSchemaToken::Any, s6);

    machine.addTransition(s2, XsdSchemaToken::Element, s2);
    machine.addTransition(s2, XsdSchemaToken::Group, s3);
    machine.addTransition(s2, XsdSchemaToken::Choice, s4);
    machine.addTransition(s2, XsdSchemaToken::Sequence, s5);
    machine.addTransition(s2, XsdSchemaToken::Any, s6);

    machine.addTransition(s3, XsdSchemaToken::Element, s2);
    machine.addTransition(s3, XsdSchemaToken::Group, s3);
    machine.addTransition(s3, XsdSchemaToken::Choice, s4);
    machine.addTransition(s3, XsdSchemaToken::Sequence, s5);
    machine.addTransition(s3, XsdSchemaToken::Any, s6);

    machine.addTransition(s4, XsdSchemaToken::Element, s2);
    machine.addTransition(s4, XsdSchemaToken::Group, s3);
    machine.addTransition(s4, XsdSchemaToken::Choice, s4);
    machine.addTransition(s4, XsdSchemaToken::Sequence, s5);
    machine.addTransition(s4, XsdSchemaToken::Any, s6);

    machine.addTransition(s5, XsdSchemaToken::Element, s2);
    machine.addTransition(s5, XsdSchemaToken::Group, s3);
    machine.addTransition(s5, XsdSchemaToken::Choice, s4);
    machine.addTransition(s5, XsdSchemaToken::Sequence, s5);
    machine.addTransition(s5, XsdSchemaToken::Any, s6);

    machine.addTransition(s6, XsdSchemaToken::Element, s2);
    machine.addTransition(s6, XsdSchemaToken::Group, s3);
    machine.addTransition(s6, XsdSchemaToken::Choice, s4);
    machine.addTransition(s6, XsdSchemaToken::Sequence, s5);
    machine.addTransition(s6, XsdSchemaToken::Any, s6);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6, data7, data8, data9;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Element << XsdSchemaToken::Sequence << XsdSchemaToken::Choice;
    data3 << XsdSchemaToken::Group;
    data4 << XsdSchemaToken::Element << XsdSchemaToken::Sequence << XsdSchemaToken::Annotation;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest8()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?) : any/selector/field/notation/include/import/referred attribute group/anyAttribute/all facets
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Element;
    data3 << XsdSchemaToken::Group;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == false);
    QVERIFY(runTest(data3, machine) == false);
    QVERIFY(runTest(data4, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest9()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (selector, field+)) : unique/key/keyref
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::InternalState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::InternalState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Selector, s2);

    machine.addTransition(s1, XsdSchemaToken::Selector, s2);
    machine.addTransition(s2, XsdSchemaToken::Field, s3);
    machine.addTransition(s3, XsdSchemaToken::Field, s3);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Selector;
    data3 << XsdSchemaToken::Annotation << XsdSchemaToken::Selector << XsdSchemaToken::Field;
    data4 << XsdSchemaToken::Selector << XsdSchemaToken::Field;
    data5 << XsdSchemaToken::Selector << XsdSchemaToken::Field << XsdSchemaToken::Field;

    QVERIFY(runTest(data1, machine) == false);
    QVERIFY(runTest(data2, machine) == false);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest10()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (appinfo | documentation)* : annotation
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Appinfo, s1);
    machine.addTransition(startState, XsdSchemaToken::Documentation, s2);

    machine.addTransition(s1, XsdSchemaToken::Appinfo, s1);
    machine.addTransition(s1, XsdSchemaToken::Documentation, s2);

    machine.addTransition(s2, XsdSchemaToken::Appinfo, s1);
    machine.addTransition(s2, XsdSchemaToken::Documentation, s2);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5;

    data1 << XsdSchemaToken::Appinfo;
    data2 << XsdSchemaToken::Appinfo << XsdSchemaToken::Appinfo;
    data3 << XsdSchemaToken::Documentation << XsdSchemaToken::Appinfo << XsdSchemaToken::Documentation;
    data4 << XsdSchemaToken::Selector << XsdSchemaToken::Field;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == false);
    QVERIFY(runTest(data5, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest11()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (restriction | list | union)) : simpleType
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::InternalState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Restriction, s2);
    machine.addTransition(startState, XsdSchemaToken::List, s3);
    machine.addTransition(startState, XsdSchemaToken::Union, s4);

    machine.addTransition(s1, XsdSchemaToken::Restriction, s2);
    machine.addTransition(s1, XsdSchemaToken::List, s3);
    machine.addTransition(s1, XsdSchemaToken::Union, s4);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6, data7, data8;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Restriction;
    data3 << XsdSchemaToken::Annotation << XsdSchemaToken::List;
    data4 << XsdSchemaToken::Annotation << XsdSchemaToken::Union;
    data5 << XsdSchemaToken::Restriction;
    data6 << XsdSchemaToken::List;
    data7 << XsdSchemaToken::Union;
    data8 << XsdSchemaToken::Union << XsdSchemaToken::Union;

    QVERIFY(runTest(data1, machine) == false);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == true);
    QVERIFY(runTest(data6, machine) == true);
    QVERIFY(runTest(data7, machine) == true);
    QVERIFY(runTest(data8, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest12()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (simpleType?, (minExclusive | minInclusive | maxExclusive | maxInclusive | totalDigits | fractionDigits | length | minLength | maxLength | enumeration | whiteSpace | pattern)*)) : simple type restriction
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(startState, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(startState, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(startState, XsdSchemaToken::Length, s3);
    machine.addTransition(startState, XsdSchemaToken::MinLength, s3);
    machine.addTransition(startState, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(startState, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(startState, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(startState, XsdSchemaToken::Pattern, s3);

    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s1, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(s1, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(s1, XsdSchemaToken::Length, s3);
    machine.addTransition(s1, XsdSchemaToken::MinLength, s3);
    machine.addTransition(s1, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(s1, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(s1, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(s1, XsdSchemaToken::Pattern, s3);

    machine.addTransition(s2, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(s2, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(s2, XsdSchemaToken::Length, s3);
    machine.addTransition(s2, XsdSchemaToken::MinLength, s3);
    machine.addTransition(s2, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(s2, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(s2, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(s2, XsdSchemaToken::Pattern, s3);

    machine.addTransition(s3, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(s3, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(s3, XsdSchemaToken::Length, s3);
    machine.addTransition(s3, XsdSchemaToken::MinLength, s3);
    machine.addTransition(s3, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(s3, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(s3, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(s3, XsdSchemaToken::Pattern, s3);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Length << XsdSchemaToken::MaxLength;
    data3 << XsdSchemaToken::Annotation << XsdSchemaToken::List;
    data4 << XsdSchemaToken::SimpleType << XsdSchemaToken::Pattern;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == false);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest13()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, simpleType?) : list
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);

    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::SimpleType;
    data3 << XsdSchemaToken::SimpleType;
    data4 << XsdSchemaToken::SimpleType << XsdSchemaToken::SimpleType;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == false);
    QVERIFY(runTest(data5, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest14()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, simpleType*) : union
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);

    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s2, XsdSchemaToken::SimpleType, s2);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::SimpleType;
    data3 << XsdSchemaToken::SimpleType;
    data4 << XsdSchemaToken::SimpleType << XsdSchemaToken::SimpleType;
    data6 << XsdSchemaToken::Annotation << XsdSchemaToken::Annotation;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == true);
    QVERIFY(runTest(data6, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest15()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for ((include | import | redefine | annotation)*, (((simpleType | complexType | group | attributeGroup) | element | attribute | notation), annotation*)*) : schema
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Include, s1);
    machine.addTransition(startState, XsdSchemaToken::Import, s1);
    machine.addTransition(startState, XsdSchemaToken::Redefine, s1);
    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(startState, XsdSchemaToken::ComplexType, s2);
    machine.addTransition(startState, XsdSchemaToken::Group, s2);
    machine.addTransition(startState, XsdSchemaToken::AttributeGroup, s2);
    machine.addTransition(startState, XsdSchemaToken::Element, s2);
    machine.addTransition(startState, XsdSchemaToken::Attribute, s2);
    machine.addTransition(startState, XsdSchemaToken::Notation, s2);

    machine.addTransition(s1, XsdSchemaToken::Include, s1);
    machine.addTransition(s1, XsdSchemaToken::Import, s1);
    machine.addTransition(s1, XsdSchemaToken::Redefine, s1);
    machine.addTransition(s1, XsdSchemaToken::Annotation, s1);
    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s1, XsdSchemaToken::ComplexType, s2);
    machine.addTransition(s1, XsdSchemaToken::Group, s2);
    machine.addTransition(s1, XsdSchemaToken::AttributeGroup, s2);
    machine.addTransition(s1, XsdSchemaToken::Element, s2);
    machine.addTransition(s1, XsdSchemaToken::Attribute, s2);
    machine.addTransition(s1, XsdSchemaToken::Notation, s2);

    machine.addTransition(s2, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s2, XsdSchemaToken::ComplexType, s2);
    machine.addTransition(s2, XsdSchemaToken::Group, s2);
    machine.addTransition(s2, XsdSchemaToken::AttributeGroup, s2);
    machine.addTransition(s2, XsdSchemaToken::Element, s2);
    machine.addTransition(s2, XsdSchemaToken::Attribute, s2);
    machine.addTransition(s2, XsdSchemaToken::Notation, s2);
    machine.addTransition(s2, XsdSchemaToken::Annotation, s3);

    machine.addTransition(s3, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s3, XsdSchemaToken::ComplexType, s2);
    machine.addTransition(s3, XsdSchemaToken::Group, s2);
    machine.addTransition(s3, XsdSchemaToken::AttributeGroup, s2);
    machine.addTransition(s3, XsdSchemaToken::Element, s2);
    machine.addTransition(s3, XsdSchemaToken::Attribute, s2);
    machine.addTransition(s3, XsdSchemaToken::Notation, s2);
    machine.addTransition(s3, XsdSchemaToken::Annotation, s3);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6;

    data1 << XsdSchemaToken::Annotation << XsdSchemaToken::Import << XsdSchemaToken::SimpleType << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::SimpleType;
    data3 << XsdSchemaToken::Annotation << XsdSchemaToken::Import << XsdSchemaToken::Include << XsdSchemaToken::Import;
    data4 << XsdSchemaToken::SimpleType << XsdSchemaToken::ComplexType << XsdSchemaToken::Annotation << XsdSchemaToken::Attribute;
    data5 << XsdSchemaToken::SimpleType << XsdSchemaToken::Include << XsdSchemaToken::Annotation << XsdSchemaToken::Attribute;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == false);
    QVERIFY(runTest(data6, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest16()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation | (simpleType | complexType | group | attributeGroup))* : redefine
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s1);
    machine.addTransition(startState, XsdSchemaToken::ComplexType, s1);
    machine.addTransition(startState, XsdSchemaToken::Group, s1);
    machine.addTransition(startState, XsdSchemaToken::AttributeGroup, s1);

    machine.addTransition(s1, XsdSchemaToken::Annotation, s1);
    machine.addTransition(s1, XsdSchemaToken::SimpleType, s1);
    machine.addTransition(s1, XsdSchemaToken::ComplexType, s1);
    machine.addTransition(s1, XsdSchemaToken::Group, s1);
    machine.addTransition(s1, XsdSchemaToken::AttributeGroup, s1);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::SimpleType;
    data3 << XsdSchemaToken::SimpleType;
    data4 << XsdSchemaToken::SimpleType << XsdSchemaToken::SimpleType;
    data6 << XsdSchemaToken::Annotation << XsdSchemaToken::Annotation;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == true);
    QVERIFY(runTest(data6, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest17()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (restriction | extension)) : simpleContent
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::InternalState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Restriction, s2);
    machine.addTransition(startState, XsdSchemaToken::Extension, s2);

    machine.addTransition(s1, XsdSchemaToken::Restriction, s2);
    machine.addTransition(s1, XsdSchemaToken::Extension, s2);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Extension;
    data3 << XsdSchemaToken::Restriction;
    data4 << XsdSchemaToken::Extension << XsdSchemaToken::Restriction;
    data5 << XsdSchemaToken::Annotation << XsdSchemaToken::Annotation;

    QVERIFY(runTest(data1, machine) == false);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == true);
    QVERIFY(runTest(data4, machine) == false);
    QVERIFY(runTest(data5, machine) == false);
    QVERIFY(runTest(data6, machine) == false);
}

void tst_XMLPatternsSchema::stateMachineTest18()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (simpleType?, (minExclusive | minInclusive | maxExclusive | maxInclusive | totalDigits | fractionDigits | length | minLength | maxLength | enumeration | whiteSpace | pattern)*)?, ((attribute | attributeGroup)*, anyAttribute?)) : simpleContent restriction
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s5 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(startState, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(startState, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(startState, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(startState, XsdSchemaToken::Length, s3);
    machine.addTransition(startState, XsdSchemaToken::MinLength, s3);
    machine.addTransition(startState, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(startState, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(startState, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(startState, XsdSchemaToken::Pattern, s3);
    machine.addTransition(startState, XsdSchemaToken::Attribute, s4);
    machine.addTransition(startState, XsdSchemaToken::AttributeGroup, s4);
    machine.addTransition(startState, XsdSchemaToken::AnyAttribute, s5);

    machine.addTransition(s1, XsdSchemaToken::SimpleType, s2);
    machine.addTransition(s1, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(s1, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(s1, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(s1, XsdSchemaToken::Length, s3);
    machine.addTransition(s1, XsdSchemaToken::MinLength, s3);
    machine.addTransition(s1, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(s1, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(s1, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(s1, XsdSchemaToken::Pattern, s3);
    machine.addTransition(s1, XsdSchemaToken::Attribute, s4);
    machine.addTransition(s1, XsdSchemaToken::AttributeGroup, s4);
    machine.addTransition(s1, XsdSchemaToken::AnyAttribute, s5);

    machine.addTransition(s2, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(s2, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(s2, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(s2, XsdSchemaToken::Length, s3);
    machine.addTransition(s2, XsdSchemaToken::MinLength, s3);
    machine.addTransition(s2, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(s2, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(s2, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(s2, XsdSchemaToken::Pattern, s3);
    machine.addTransition(s2, XsdSchemaToken::Attribute, s4);
    machine.addTransition(s2, XsdSchemaToken::AttributeGroup, s4);
    machine.addTransition(s2, XsdSchemaToken::AnyAttribute, s5);

    machine.addTransition(s3, XsdSchemaToken::MinExclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::MinInclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::MaxExclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::MaxInclusive, s3);
    machine.addTransition(s3, XsdSchemaToken::TotalDigits, s3);
    machine.addTransition(s3, XsdSchemaToken::FractionDigits, s3);
    machine.addTransition(s3, XsdSchemaToken::Length, s3);
    machine.addTransition(s3, XsdSchemaToken::MinLength, s3);
    machine.addTransition(s3, XsdSchemaToken::MaxLength, s3);
    machine.addTransition(s3, XsdSchemaToken::Enumeration, s3);
    machine.addTransition(s3, XsdSchemaToken::WhiteSpace, s3);
    machine.addTransition(s3, XsdSchemaToken::Pattern, s3);
    machine.addTransition(s3, XsdSchemaToken::Attribute, s4);
    machine.addTransition(s3, XsdSchemaToken::AttributeGroup, s4);
    machine.addTransition(s3, XsdSchemaToken::AnyAttribute, s5);

    machine.addTransition(s4, XsdSchemaToken::Attribute, s4);
    machine.addTransition(s4, XsdSchemaToken::AttributeGroup, s4);
    machine.addTransition(s4, XsdSchemaToken::AnyAttribute, s5);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::SimpleType << XsdSchemaToken::MinExclusive << XsdSchemaToken::MaxExclusive;
    data3 << XsdSchemaToken::AnyAttribute << XsdSchemaToken::Attribute;
    data4 << XsdSchemaToken::MinExclusive << XsdSchemaToken::TotalDigits << XsdSchemaToken::Enumeration;
    data5 << XsdSchemaToken::Annotation << XsdSchemaToken::Annotation;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == false);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == false);
    QVERIFY(runTest(data6, machine) == true);
}

void tst_XMLPatternsSchema::stateMachineTest19()
{
    XsdStateMachine<XsdSchemaToken::NodeName> machine;

    // setup state machine for (annotation?, (group | all | choice | sequence)?, ((attribute | attributeGroup)*, anyAttribute?)) : complex content restriction/complex content extension
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId startState = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::StartEndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s1 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s2 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s3 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);
    const XsdStateMachine<XsdSchemaToken::NodeName>::StateId s4 = machine.addState(XsdStateMachine<XsdSchemaToken::NodeName>::EndState);

    machine.addTransition(startState, XsdSchemaToken::Annotation, s1);
    machine.addTransition(startState, XsdSchemaToken::Group, s2);
    machine.addTransition(startState, XsdSchemaToken::All, s2);
    machine.addTransition(startState, XsdSchemaToken::Choice, s2);
    machine.addTransition(startState, XsdSchemaToken::Sequence, s2);
    machine.addTransition(startState, XsdSchemaToken::Attribute, s3);
    machine.addTransition(startState, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(startState, XsdSchemaToken::AnyAttribute, s4);

    machine.addTransition(s1, XsdSchemaToken::Group, s2);
    machine.addTransition(s1, XsdSchemaToken::All, s2);
    machine.addTransition(s1, XsdSchemaToken::Choice, s2);
    machine.addTransition(s1, XsdSchemaToken::Sequence, s2);
    machine.addTransition(s1, XsdSchemaToken::Attribute, s3);
    machine.addTransition(s1, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(s1, XsdSchemaToken::AnyAttribute, s4);

    machine.addTransition(s2, XsdSchemaToken::Attribute, s3);
    machine.addTransition(s2, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(s2, XsdSchemaToken::AnyAttribute, s4);

    machine.addTransition(s3, XsdSchemaToken::Attribute, s3);
    machine.addTransition(s3, XsdSchemaToken::AttributeGroup, s3);
    machine.addTransition(s3, XsdSchemaToken::AnyAttribute, s4);

    QVector<XsdSchemaToken::NodeName> data1, data2, data3, data4, data5, data6;

    data1 << XsdSchemaToken::Annotation;
    data2 << XsdSchemaToken::Annotation << XsdSchemaToken::Group;
    data3 << XsdSchemaToken::Annotation << XsdSchemaToken::Group << XsdSchemaToken::Sequence;
    data4 << XsdSchemaToken::Attribute << XsdSchemaToken::Attribute;
    data5 << XsdSchemaToken::Attribute << XsdSchemaToken::Sequence;
    data6 << XsdSchemaToken::Annotation << XsdSchemaToken::Annotation;

    QVERIFY(runTest(data1, machine) == true);
    QVERIFY(runTest(data2, machine) == true);
    QVERIFY(runTest(data3, machine) == false);
    QVERIFY(runTest(data4, machine) == true);
    QVERIFY(runTest(data5, machine) == false);
    QVERIFY(runTest(data6, machine) == false);
}

QTEST_MAIN(tst_XMLPatternsSchema)
#include "tst_xmlpatternsschema.moc"
