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
#include <qdeclarativedatatest.h>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QDeclarativeEngine>
#include <QDeclarativeComponent>
#include <QtDeclarative/qdeclarative.h>
#include <QtDeclarative/qdeclarativeprivate.h>
#include <QtDeclarative/qdeclarativeproperty.h>
#include <QDebug>

class tst_qdeclarativelistreference : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativelistreference() {}

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
    Q_PROPERTY(QDeclarativeListProperty<TestType> data READ dataProperty)
    Q_PROPERTY(int intProperty READ intProperty)

public:
    TestType() : property(this, data) {}
    QDeclarativeListProperty<TestType> dataProperty() { return property; }
    int intProperty() const { return 10; }

    QList<TestType *> data;
    QDeclarativeListProperty<TestType> property;
};

void tst_qdeclarativelistreference::initTestCase()
{
    QDeclarativeDataTest::initTestCase();
    qmlRegisterType<TestType>();
}

void tst_qdeclarativelistreference::qmllistreference()
{
    TestType tt;

    QDeclarativeListReference r(&tt, "data");
    QVERIFY(r.isValid() == true);
    QCOMPARE(r.count(), 0);

    tt.data.append(&tt);
    QCOMPARE(r.count(), 1);
}

void tst_qdeclarativelistreference::qmllistreference_invalid()
{
    TestType tt;

    // Invalid
    {
    QDeclarativeListReference r;
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
    }

    // Non-property
    {
    QDeclarativeListReference r(&tt, "blah");
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
    }

    // Non-list property
    {
    QDeclarativeListReference r(&tt, "intProperty");
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
    }
}

void tst_qdeclarativelistreference::isValid()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.isValid() == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.isValid() == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.isValid() == true);
    delete tt;
    QVERIFY(ref.isValid() == false);
    }
}

void tst_qdeclarativelistreference::object()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.object() == 0);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.object() == 0);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.object() == tt);
    delete tt;
    QVERIFY(ref.object() == 0);
    }
}

void tst_qdeclarativelistreference::listElementType()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.listElementType() == 0);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.listElementType() == 0);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.listElementType() == &TestType::staticMetaObject);
    delete tt;
    QVERIFY(ref.listElementType() == 0);
    }
}

void tst_qdeclarativelistreference::canAppend()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.canAppend() == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.canAppend() == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.canAppend() == true);
    delete tt;
    QVERIFY(ref.canAppend() == false);
    }

    {
    TestType tt;
    tt.property.append = 0;
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.canAppend() == false);
    }
}

void tst_qdeclarativelistreference::canAt()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.canAt() == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.canAt() == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.canAt() == true);
    delete tt;
    QVERIFY(ref.canAt() == false);
    }

    {
    TestType tt;
    tt.property.at = 0;
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.canAt() == false);
    }
}

void tst_qdeclarativelistreference::canClear()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.canClear() == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.canClear() == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.canClear() == true);
    delete tt;
    QVERIFY(ref.canClear() == false);
    }

    {
    TestType tt;
    tt.property.clear = 0;
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.canClear() == false);
    }
}

void tst_qdeclarativelistreference::canCount()
{
    TestType *tt = new TestType;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.canCount() == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.canCount() == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.canCount() == true);
    delete tt;
    QVERIFY(ref.canCount() == false);
    }

    {
    TestType tt;
    tt.property.count = 0;
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.canCount() == false);
    }
}

void tst_qdeclarativelistreference::append()
{
    TestType *tt = new TestType;
    QObject object;

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.append(tt) == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.append(tt) == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
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
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.append(&tt) == false);
    }
}

void tst_qdeclarativelistreference::at()
{
    TestType *tt = new TestType;
    tt->data.append(tt);
    tt->data.append(0);
    tt->data.append(tt);

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.at(0) == 0);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.at(0) == 0);
    }

    {
    QDeclarativeListReference ref(tt, "data");
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
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.at(0) == 0);
    }
}

void tst_qdeclarativelistreference::clear()
{
    TestType *tt = new TestType;
    tt->data.append(tt);
    tt->data.append(0);
    tt->data.append(tt);

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.clear() == false);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.clear() == false);
    }

    {
    QDeclarativeListReference ref(tt, "data");
    QVERIFY(ref.clear() == true);
    QVERIFY(tt->data.count() == 0);
    delete tt;
    QVERIFY(ref.clear() == false);
    }

    {
    TestType tt;
    tt.property.clear = 0;
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.clear() == false);
    }
}

void tst_qdeclarativelistreference::count()
{
    TestType *tt = new TestType;
    tt->data.append(tt);
    tt->data.append(0);
    tt->data.append(tt);

    {
    QDeclarativeListReference ref;
    QVERIFY(ref.count() == 0);
    }

    {
    QDeclarativeListReference ref(tt, "blah");
    QVERIFY(ref.count() == 0);
    }

    {
    QDeclarativeListReference ref(tt, "data");
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
    QDeclarativeListReference ref(&tt, "data");
    QVERIFY(ref.count() == 0);
    }
}

void tst_qdeclarativelistreference::copy()
{
    TestType tt;
    tt.data.append(&tt);
    tt.data.append(0);
    tt.data.append(&tt);

    QDeclarativeListReference *r1 = new QDeclarativeListReference(&tt, "data");
    QVERIFY(r1->count() == 3);

    QDeclarativeListReference r2(*r1);
    QDeclarativeListReference r3;
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

void tst_qdeclarativelistreference::qmlmetaproperty()
{
    TestType tt;
    tt.data.append(&tt);
    tt.data.append(0);
    tt.data.append(&tt);

    QDeclarativeProperty prop(&tt, QLatin1String("data"));
    QVariant v = prop.read();
    QVERIFY(v.userType() == qMetaTypeId<QDeclarativeListReference>());
    QDeclarativeListReference ref = qvariant_cast<QDeclarativeListReference>(v);
    QVERIFY(ref.count() == 3);
    QVERIFY(ref.listElementType() == &TestType::staticMetaObject);
}

void tst_qdeclarativelistreference::engineTypes()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("engineTypes.qml"));

    QObject *o = component.create();
    QVERIFY(o);

    QDeclarativeProperty p1(o, QLatin1String("myList"));
    QVERIFY(p1.propertyTypeCategory() == QDeclarativeProperty::Normal);

    QDeclarativeProperty p2(o, QLatin1String("myList"), engine.rootContext());
    QVERIFY(p2.propertyTypeCategory() == QDeclarativeProperty::List);
    QVariant v = p2.read();
    QVERIFY(v.userType() == qMetaTypeId<QDeclarativeListReference>());
    QDeclarativeListReference ref = qvariant_cast<QDeclarativeListReference>(v);
    QVERIFY(ref.count() == 2);
    QVERIFY(ref.listElementType());
    QVERIFY(ref.listElementType() != &QObject::staticMetaObject);

    delete o;
}

void tst_qdeclarativelistreference::variantToList()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine, testFileUrl("variantToList.qml"));

    QObject *o = component.create();
    QVERIFY(o);

    QVERIFY(o->property("value").userType() == qMetaTypeId<QDeclarativeListReference>());
    QCOMPARE(o->property("test").toInt(), 1);

    delete o;
}

QTEST_MAIN(tst_qdeclarativelistreference)

#include "tst_qdeclarativelistreference.moc"
