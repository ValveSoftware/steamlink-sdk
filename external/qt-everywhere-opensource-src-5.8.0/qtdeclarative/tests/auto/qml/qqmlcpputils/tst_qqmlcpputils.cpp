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
#include <qsignalspy.h>
#include <private/qqmlglobal_p.h>

class tst_qqmlcpputils : public QObject
{
    Q_OBJECT
public:
    tst_qqmlcpputils() {}

private slots:
    void fastConnect();
    void fastCast();
};

class MyObject : public QObject {
    Q_OBJECT
public:
    MyObject() : slotCount(0) {}
    friend class tst_qqmlcpputils;

    int slotCount;

signals:
    void signal1();
    void signal2();

public slots:
    void slot1() { slotCount++; }
};

void tst_qqmlcpputils::fastConnect()
{
    {
        MyObject *obj = new MyObject;
        qmlobject_connect(obj, MyObject, SIGNAL(signal1()), obj, MyObject, SLOT(slot1()));

        obj->signal1();
        QCOMPARE(obj->slotCount, 1);

        delete obj;
    }

    {
        MyObject obj;
        qmlobject_connect(&obj, MyObject, SIGNAL(signal1()), &obj, MyObject, SLOT(slot1()))

        obj.signal1();
        QCOMPARE(obj.slotCount, 1);
    }

    {
        MyObject *obj = new MyObject;
        QSignalSpy spy(obj, SIGNAL(signal2()));
        qmlobject_connect(obj, MyObject, SIGNAL(signal1()), obj, MyObject, SIGNAL(signal2()));

        obj->signal1();
        QCOMPARE(spy.count(), 1);

        delete obj;
    }
}

void tst_qqmlcpputils::fastCast()
{
    {
        QObject *myObj = new MyObject;
        MyObject *obj = qmlobject_cast<MyObject*>(myObj);
        QVERIFY(obj);
        QCOMPARE(obj->metaObject(), myObj->metaObject());
        obj->slot1();
        QCOMPARE(obj->slotCount, 1);
        delete myObj;
    }

    {
        QObject *nullObj = 0;
        QObject *obj = qmlobject_cast<QObject *>(nullObj);
        QCOMPARE(obj, nullObj); // shouldn't crash/assert.
    }
}

QTEST_MAIN(tst_qqmlcpputils)

#include "tst_qqmlcpputils.moc"
