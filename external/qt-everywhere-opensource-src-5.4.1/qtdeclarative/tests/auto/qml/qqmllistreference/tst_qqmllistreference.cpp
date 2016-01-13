/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
    QVERIFY(r.isValid() == true);
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
    QVERIFY(r.isValid() == false);
    QVERIFY(r.object() == 0);
    QVERIFY(r.listElementType() == 0);
    QVERIFY(r.canAt() == false);
    QVERIFY(r.canClear() == false);
    QVERIFY(r.canCount() == false);
    QVERIFY(r.append(0) == false);
    QVERIFY(r.at(10) == 0);
    QVERIFY(r.clear() == false);
    QVERIFY(r.count() == 0);
    QVERIFY(r.isReadable() == false);
    QVERIFY(r.isManipulable() == false);
    }

    // Non-property
    {
    QQmlListReference r(&tt, "blah");
    QVERIFY(r.isValid() == false);
    QVERIFY(r.object() == 0);
    QVERIFY(r.listElementType() == 0);
    QVERIFY(r.canAt() == false);
    QVERIFY(r.canClear() == false);
    QVERIFY(r.canCount() == false);
    QVERIFY(r.append(0) == false);
    QVERIFY(r.at(10) == 0);
    QVERIFY(r.clear() == false);
    QVERIFY(r.count() == 0);
    QVERIFY(r.isReadable() == false);
    QVERIFY(r.isManipulable() == false);
    }

    // Non-list property
    {
    QQmlListReference r(&tt, "intProperty");
    QVERIFY(r.isValid() == false);
    QVERIFY(r.object() == 0);
    QVERIFY(r.listElementType() == 0);
    QVERIFY(r.canAt() == false);
    QVERIFY(r.canClear() == false);
    QVERIFY(r.canCount() == false);
    QVERIFY(r.append(0) == false);
    QVERIFY(r.at(10) == 0);
    QVERIFY(r.clear() == false);
    QVERIFY(r.count() == 0);
    QVERIFY(r.isReadable() == false);
    QVERIFY(r.isManipulable() == false);
    }
}

void tst_qqmllistreference::isValid()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.isValid() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.isValid() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.isValid() == true);
    delete tt;
    QVERIFY(ref.isValid() == false);
    }
}

void tst_qqmllistreference::object()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.object() == 0);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.object() == 0);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.object() == tt);
    delete tt;
    QVERIFY(ref.object() == 0);
    }
}

void tst_qqmllistreference::listElementType()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.listElementType() == 0);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.listElementType() == 0);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.listElementType() == &TestType::staticMetaObject);
    delete tt;
    QVERIFY(ref.listElementType() == 0);
    }
}

void tst_qqmllistreference::canAppend()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.canAppend() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.canAppend() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canAppend() == true);
    delete tt;
    QVERIFY(ref.canAppend() == false);
    }

    {
    TestType tt;
    tt.property.append = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.canAppend() == false);
    }
}

void tst_qqmllistreference::canAt()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.canAt() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.canAt() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canAt() == true);
    delete tt;
    QVERIFY(ref.canAt() == false);
    }

    {
    TestType tt;
    tt.property.at = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.canAt() == false);
    }
}

void tst_qqmllistreference::canClear()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.canClear() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.canClear() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canClear() == true);
    delete tt;
    QVERIFY(ref.canClear() == false);
    }

    {
    TestType tt;
    tt.property.clear = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.canClear() == false);
    }
}

void tst_qqmllistreference::canCount()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.canCount() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.canCount() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.canCount() == true);
    delete tt;
    QVERIFY(ref.canCount() == false);
    }

    {
    TestType tt;
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.canCount() == false);
    }
}

void tst_qqmllistreference::isReadable()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.isReadable() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.isReadable() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.isReadable() == true);
    delete tt;
    QVERIFY(ref.isReadable() == false);
    }

    {
    TestType tt;
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.isReadable() == false);
    }
}

void tst_qqmllistreference::isManipulable()
{
    TestType *tt = new TestType;

    {
    QQmlListReference ref;
    QVERIFY(ref.isManipulable() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.isManipulable() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.isManipulable() == true);
    delete tt;
    QVERIFY(ref.isManipulable() == false);
    }

    {
    TestType tt;
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.isManipulable() == false);
    }
}

void tst_qqmllistreference::append()
{
    TestType *tt = new TestType;
    QObject object;

    {
    QQmlListReference ref;
    QVERIFY(ref.append(tt) == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.append(tt) == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.append(tt) == true);
    QVERIFY(tt->data.count() == 1);
    QVERIFY(tt->data.at(0) == tt);
    QVERIFY(ref.append(&object) == false);
    QVERIFY(tt->data.count() == 1);
    QVERIFY(tt->data.at(0) == tt);
    QVERIFY(ref.append(0) == true);
    QVERIFY(tt->data.count() == 2);
    QVERIFY(tt->data.at(0) == tt);
    QVERIFY(tt->data.at(1) == 0);
    delete tt;
    QVERIFY(ref.append(0) == false);
    }

    {
    TestType tt;
    tt.property.append = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.append(&tt) == false);
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
    QVERIFY(ref.at(0) == 0);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.at(0) == 0);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.at(0) == tt);
    QVERIFY(ref.at(1) == 0);
    QVERIFY(ref.at(2) == tt);
    delete tt;
    QVERIFY(ref.at(0) == 0);
    }

    {
    TestType tt;
    tt.data.append(&tt);
    tt.property.at = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.at(0) == 0);
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
    QVERIFY(ref.clear() == false);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.clear() == false);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.clear() == true);
    QVERIFY(tt->data.count() == 0);
    delete tt;
    QVERIFY(ref.clear() == false);
    }

    {
    TestType tt;
    tt.property.clear = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.clear() == false);
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
    QVERIFY(ref.count() == 0);
    }

    {
    QQmlListReference ref(tt, "blah");
    QVERIFY(ref.count() == 0);
    }

    {
    QQmlListReference ref(tt, "data");
    QVERIFY(ref.count() == 3);
    tt->data.removeAt(1);
    QVERIFY(ref.count() == 2);
    delete tt;
    QVERIFY(ref.count() == 0);
    }

    {
    TestType tt;
    tt.data.append(&tt);
    tt.property.count = 0;
    QQmlListReference ref(&tt, "data");
    QVERIFY(ref.count() == 0);
    }
}

void tst_qqmllistreference::copy()
{
    TestType tt;
    tt.data.append(&tt);
    tt.data.append(0);
    tt.data.append(&tt);

    QQmlListReference *r1 = new QQmlListReference(&tt, "data");
    QVERIFY(r1->count() == 3);

    QQmlListReference r2(*r1);
    QQmlListReference r3;
    r3 = *r1;

    QVERIFY(r2.count() == 3);
    QVERIFY(r3.count() == 3);

    delete r1;

    QVERIFY(r2.count() == 3);
    QVERIFY(r3.count() == 3);

    tt.data.removeAt(2);

    QVERIFY(r2.count() == 2);
    QVERIFY(r3.count() == 2);
}

void tst_qqmllistreference::qmlmetaproperty()
{
    TestType tt;
    tt.data.append(&tt);
    tt.data.append(0);
    tt.data.append(&tt);

    QQmlProperty prop(&tt, QLatin1String("data"));
    QVariant v = prop.read();
    QVERIFY(v.userType() == qMetaTypeId<QQmlListReference>());
    QQmlListReference ref = qvariant_cast<QQmlListReference>(v);
    QVERIFY(ref.count() == 3);
    QVERIFY(ref.listElementType() == &TestType::staticMetaObject);
}

void tst_qqmllistreference::engineTypes()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("engineTypes.qml"));

    QObject *o = component.create();
    QVERIFY(o);

    QQmlProperty p1(o, QLatin1String("myList"));
    QVERIFY(p1.propertyTypeCategory() == QQmlProperty::List);

    QQmlProperty p2(o, QLatin1String("myList"), engine.rootContext());
    QVERIFY(p2.propertyTypeCategory() == QQmlProperty::List);
    QVariant v = p2.read();
    QVERIFY(v.userType() == qMetaTypeId<QQmlListReference>());
    QQmlListReference ref = qvariant_cast<QQmlListReference>(v);
    QVERIFY(ref.count() == 2);
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

    QVERIFY(o->property("value").userType() == qMetaTypeId<QQmlListReference>());
    QCOMPARE(o->property("test").toInt(), 1);

    delete o;
}

QTEST_MAIN(tst_qqmllistreference)

#include "tst_qqmllistreference.moc"
