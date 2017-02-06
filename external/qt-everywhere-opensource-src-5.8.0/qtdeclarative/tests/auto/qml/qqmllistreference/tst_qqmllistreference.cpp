/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QtQml/qqml.h>
#include <QtQml/qqmlprivate.h>
#include <QtQml/qqmlproperty.h>
#include <QDebug>
#include "../../shared/util.h"

class tst_qqmllistreference : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmllistreference() {}

private slots:
    void initTestCase();
    void qmllistreference();
    void qmllistreference_invalid();
    void isValid();
    void object();
    void listElementType();
    void canAppend();
    void canAt();
    void canClear();
    void canCount();
    void isReadable();
    void isManipulable();
    void append();
    void at();
    void clear();
    void count();
    void copy();
    void qmlmetaproperty();
    void engineTypes();
    void variantToList();
};

class TestType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<TestType> data READ dataProperty)
    Q_PROPERTY(int intProperty READ intProperty)

public:
    TestType() : property(this, data) {}
    QQmlListProperty<TestType> dataProperty() { return property; }
    int intProperty() const { return 10; }

    QList<TestType *> data;
    QQmlListProperty<TestType> property;
};

void tst_qqmllistreference::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<TestType>();
}

void tst_qqmllistreference::qmllistreference()
{
    TestType tt;

    QQmlListReference r(&tt, "data");
    QVERIFY(r.isValid());
    QCOMPARE(r.count(), 0);

    tt.data.append(&tt);
    QCOMPARE(r.count(), 1);
}

void tst_qqmllistreference::qmllistreference_invalid()
{
    TestType tt;

    // Invalid
    {
    QQmlListReference r;
    QVERIFY(!r.isValid());
    QVERIFY(!r.object());
    QVERIFY(!r.listElementType());
    QVERIFY(!r.canAt());
    QVERIFY(!r.canClear());
    QVERIFY(!r.canCount());
    QVERIFY(!r.append(0));
    QVERIFY(!r.at(10));
    QVERIFY(!r.clear());
    QCOMPARE(r.count(), 0);
    QVERIFY(!r.isReadable());
    QVERIFY(!r.isManipulable());
    }

    // Non-property
    {
    QQmlListReference r(&tt, "blah");
    QVERIFY(!r.isValid());
    QVERIFY(!r.object());
    QVERIFY(!r.listElementType());
    QVERIFY(!r.canAt());
    QVERIFY(!r.canClear());
    QVERIFY(!r.canCount());
    QVERIFY(!r.append(0));
    QVERIFY(!r.at(10));
    QVERIFY(!r.clear());
    QCOMPARE(r.count(), 0);
    QVERIFY(!r.isReadable());
    QVERIFY(!r.isManipulable());
    }

    // Non-list property
    {
    QQmlListReference r(&tt, "intProperty");
    QVERIFY(!r.isValid());
    QVERIFY(!r.object());
    QVERIFY(!r.listElementType());
    QVERIFY(!r.canAt());
    QVERIFY(!r.canClear());
    QVERIFY(!r.canCount());
    QVERIFY(!r.append(0));
    QVERIFY(!r.at(10));
    QVERIFY(!r.clear());
    QCOMPARE(r.count(), 0);
    QVERIFY(!r.isReadable());
    QVERIFY(!r.isManipulable());
    }
}

void tst_qqmllistreference::isValid()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.isValid());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.isValid());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.isValid());
    delete tt;
    QVERIFY(!ref.isValid());
    }
}

void tst_qqmllistreference::object()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.object());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.object());
    }

    {
    QQmlListReference ref(tt, "data");
    QCOMPARE(ref.object(), tt);
    delete tt;
    QVERIFY(!ref.object());
    }
}

void tst_qqmllistreference::listElementType()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.listElementType());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.listElementType());
    }

    {
    QQmlListReference ref(tt, "data");
    QCOMPARE(ref.listElementType(), &TestType::staticMetaObject);
    delete tt;
    QVERIFY(!ref.listElementType());
    }
}

void tst_qqmllistreference::canAppend()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.canAppend());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.canAppend());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canAppend());
    delete tt;
    QVERIFY(!ref.canAppend());
    }

    {
    TestType tt;
    tt.property.append = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.canAppend());
    }
}

void tst_qqmllistreference::canAt()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.canAt());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.canAt());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canAt());
    delete tt;
    QVERIFY(!ref.canAt());
    }

    {
    TestType tt;
    tt.property.at = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.canAt());
    }
}

void tst_qqmllistreference::canClear()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.canClear());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.canClear());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canClear());
    delete tt;
    QVERIFY(!ref.canClear());
    }

    {
    TestType tt;
    tt.property.clear = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.canClear());
    }
}

void tst_qqmllistreference::canCount()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.canCount());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.canCount());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canCount());
    delete tt;
    QVERIFY(!ref.canCount());
    }

    {
    TestType tt;
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.canCount());
    }
}

void tst_qqmllistreference::isReadable()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.isReadable());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.isReadable());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.isReadable());
    delete tt;
    QVERIFY(!ref.isReadable());
    }

    {
    TestType tt;
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.isReadable());
    }
}

void tst_qqmllistreference::isManipulable()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(!ref.isManipulable());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.isManipulable());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.isManipulable());
    delete tt;
    QVERIFY(!ref.isManipulable());
    }

    {
    TestType tt;
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.isManipulable());
    }
}

void tst_qqmllistreference::append()
{
    TestType *tt = new TestType;
    QObject object;

    {
    QQmlListReference ref;
    QVERIFY(!ref.append(tt));
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.append(tt));
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.append(tt));
    QCOMPARE(tt->data.count(), 1);
    QCOMPARE(tt->data.at(0), tt);
    QVERIFY(!ref.append(&object));
    QCOMPARE(tt->data.count(), 1);
    QCOMPARE(tt->data.at(0), tt);
    QVERIFY(ref.append(0));
    QCOMPARE(tt->data.count(), 2);
    QCOMPARE(tt->data.at(0), tt);
    QVERIFY(!tt->data.at(1));
    delete tt;
    QVERIFY(!ref.append(0));
    }

    {
    TestType tt;
    tt.property.append = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.append(&tt));
    }
}

void tst_qqmllistreference::at()
{
    TestType *tt = new TestType;
    tt->data.append(tt);
    tt->data.append(0);
    tt->data.append(tt);

    {
    QQmlListReference ref;
    QVERIFY(!ref.at(0));
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.at(0));
    }

    {
    QQmlListReference ref(tt, "data");
    QCOMPARE(ref.at(0), tt);
    QVERIFY(!ref.at(1));
    QCOMPARE(ref.at(2), tt);
    delete tt;
    QVERIFY(!ref.at(0));
    }

    {
    TestType tt;
    tt.data.append(&tt);
    tt.property.at = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.at(0));
    }
}

void tst_qqmllistreference::clear()
{
    TestType *tt = new TestType;
    tt->data.append(tt);
    tt->data.append(0);
    tt->data.append(tt);

    {
    QQmlListReference ref;
    QVERIFY(!ref.clear());
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(!ref.clear());
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.clear());
    QCOMPARE(tt->data.count(), 0);
    delete tt;
    QVERIFY(!ref.clear());
    }

    {
    TestType tt;
    tt.property.clear = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(!ref.clear());
    }
}

void tst_qqmllistreference::count()
{
    TestType *tt = new TestType;
    tt->data.append(tt);
    tt->data.append(0);
    tt->data.append(tt);

    {
    QQmlListReference ref;
    QCOMPARE(ref.count(), 0);
    }

    {
    QQmlListReference ref(tt, "blah");
    QCOMPARE(ref.count(), 0);
    }

    {
    QQmlListReference ref(tt, "data");
    QCOMPARE(ref.count(), 3);
    tt->data.removeAt(1);
    QCOMPARE(ref.count(), 2);
    delete tt;
    QCOMPARE(ref.count(), 0);
    }

    {
    TestType tt;
    tt.data.append(&tt);
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QCOMPARE(ref.count(), 0);
    }
}

void tst_qqmllistreference::copy()
{
    TestType tt;
    tt.data.append(&tt);
    tt.data.append(0);
    tt.data.append(&tt);

    QQmlListReference *r1 = new QQmlListReference(&tt, "data");
    QCOMPARE(r1->count(), 3);

    QQmlListReference r2(*r1);
    QQmlListReference r3;
    r3 = *r1;

    QCOMPARE(r2.count(), 3);
    QCOMPARE(r3.count(), 3);

    delete r1;

    QCOMPARE(r2.count(), 3);
    QCOMPARE(r3.count(), 3);

    tt.data.removeAt(2);

    QCOMPARE(r2.count(), 2);
    QCOMPARE(r3.count(), 2);
}

void tst_qqmllistreference::qmlmetaproperty()
{
    TestType tt;
    tt.data.append(&tt);
    tt.data.append(0);
    tt.data.append(&tt);

    QQmlProperty prop(&tt, QLatin1String("data"));
    QVariant v = prop.read();
    QCOMPARE(v.userType(), qMetaTypeId<QQmlListReference>());
    QQmlListReference ref = qvariant_cast<QQmlListReference>(v);
    QCOMPARE(ref.count(), 3);
    QCOMPARE(ref.listElementType(), &TestType::staticMetaObject);
}

void tst_qqmllistreference::engineTypes()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("engineTypes.qml"));

    QObject *o = component.create();
    QVERIFY(o);

    QQmlProperty p1(o, QLatin1String("myList"));
    QCOMPARE(p1.propertyTypeCategory(), QQmlProperty::List);

    QQmlProperty p2(o, QLatin1String("myList"), engine.rootContext());
    QCOMPARE(p2.propertyTypeCategory(), QQmlProperty::List);
    QVariant v = p2.read();
    QCOMPARE(v.userType(), qMetaTypeId<QQmlListReference>());
    QQmlListReference ref = qvariant_cast<QQmlListReference>(v);
    QCOMPARE(ref.count(), 2);
    QVERIFY(ref.listElementType());
    QVERIFY(ref.listElementType() != &QObject::staticMetaObject);

    delete o;
}

void tst_qqmllistreference::variantToList()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("variantToList.qml"));

    QObject *o = component.create();
    QVERIFY(o);

    QCOMPARE(o->property("value").userType(), qMetaTypeId<QQmlListReference>());
    QCOMPARE(o->property("test").toInt(), 1);

    delete o;
}

QTEST_MAIN(tst_qqmllistreference)

#include "tst_qqmllistreference.moc"
