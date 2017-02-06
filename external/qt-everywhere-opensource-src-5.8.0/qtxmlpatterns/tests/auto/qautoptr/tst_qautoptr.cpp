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


#include <QtTest/QtTest>

#include "private/qautoptr_p.h"

using namespace QPatternist;

/*!
 \class tst_QAutoPtr
 \internal
 \since 4.4
 \brief Tests class QAutoPtr.

 */
class tst_QAutoPtr : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstructor() const;
    void copyConstructor() const;
    void assignmentOperator() const;
    void data() const;
    void dataSignature() const;
    void release() const;
    void reset() const;
    void onConstObject() const;
    void dereferenceOperator() const;
    void pointerOperator() const;
    void pointerOperatorSignature() const;
    void negationOperator() const;
    void negationOperatorSignature() const;
    void operatorBool() const;
    void operatorBoolSignature() const;
    /*
     TODO:
     - Test all the type hierarchy operators/constructors
     - No code is at all calling AutoPtr& operator=(AutoPtrRef<T> ref) currently. Is it needed?
     - No code is at all calling AutoPtrRef stuff. Is it needed?
     - Equalness/unequalness operators?
     - Test AutoPtr& operator=(AutoPtrRef<T> ref)
     */
};

void tst_QAutoPtr::defaultConstructor() const
{
    /* Check that the members, one, is correctly initialized. */
    AutoPtr<int> p;
    QCOMPARE(p.data(), static_cast<int *>(0));
}

void tst_QAutoPtr::copyConstructor() const
{
    /* Copy default constructed value. */
    {
        AutoPtr<int> p1;
        AutoPtr<int> p2(p1);
        QCOMPARE(p2.data(), static_cast<int *>(0));
    }

    /* Copy active value. */
    {
        AutoPtr<int> p1(new int(7));
        AutoPtr<int> p2(p1);
        QCOMPARE(*p2.data(), 7);
        QCOMPARE(p1.data(), static_cast<int *>(0));
    }
}

void tst_QAutoPtr::assignmentOperator() const
{
    /* Assign to self, a default constructed value. */
    {
        AutoPtr<int> p1;
        p1 = p1;
        p1 = p1;
        p1 = p1;
    }

    /* Assign to a default constructed value. */
    {
        AutoPtr<int> p1;
        AutoPtr<int> p2;
        p1 = p2;
        p1 = p2;
        p1 = p2;
    }

    /* Assign to an active value. */
    {
        AutoPtr<int> p1(new int(6));
        AutoPtr<int> p2;
        p1 = p2;
        p1 = p2;
        p1 = p2;
    }

    /* Assign from an active value. */
    {
        int *const ptr =  new int(6);
        AutoPtr<int> p1(ptr);
        AutoPtr<int> p2;
        p2 = p1;

        QCOMPARE(p2.data(), ptr);
        /* p1 should have reset. */
        QCOMPARE(p1.data(), static_cast<int *>(0));
    }
}

void tst_QAutoPtr::data() const
{
    AutoPtr<int> p;

    QCOMPARE(p.data(), static_cast<int *>(0));
}

void tst_QAutoPtr::dataSignature() const
{
    const AutoPtr<int> p;
    /* data() should be const. */
    p.data();
}

void tst_QAutoPtr::release() const
{
    /* Call release() on a default constructed value. */
    {
        AutoPtr<int> p;
        QCOMPARE(p.release(), static_cast<int *>(0));
    }

    /* Call release() on an active value, it shouldn't delete. */
    {
        int value = 3;
        AutoPtr<int> p(&value);
        p.release();
    }
}

void tst_QAutoPtr::reset() const
{
    /* Call reset() on a default constructed value. */
    {
        AutoPtr<int> p;
        p.reset();
    }

    /* Call reset() on an active value. */
    {
        AutoPtr<int> p(new int(3));
        p.reset();
    }

    /* Call reset() with a value, on an active value. */
    {
        AutoPtr<int> p(new int(3));

        int *const value = new int(9);
        p.reset(value);
        QCOMPARE(*p.data(), 9);
    }

    /* Call reset() with a value, on default constructed value. */
    {
        AutoPtr<int> p;

        int *const value = new int(9);
        p.reset(value);
        QCOMPARE(*p.data(), 9);
    }
}

void tst_QAutoPtr::onConstObject() const
{
    /* Instansiate on a const object. */
    AutoPtr<const int> p(new int(3));
    p.reset();
    p.data();
    p.release();
    p = p;
}

class AbstractClass
{
public:
    virtual ~AbstractClass()
    {
    }

    virtual int member() const = 0;
};

class SubClass : public AbstractClass
{
public:
    virtual int member() const
    {
        return 5;
    }
};

void tst_QAutoPtr::dereferenceOperator() const
{
    /* Dereference a basic value. */
    {
        int value = 5;
        AutoPtr<int> p(&value);

        const int value2 = *p;
        QCOMPARE(value2, 5);
        p.release();
    }

    /* Dereference a pointer to an abstract class. This verifies
     * that the operator returns a reference, when compiling
     * with MSVC 2005. */
    {
        AutoPtr<AbstractClass> p(new SubClass());

        QCOMPARE((*p).member(), 5);
    }
}

class AnyForm
{
public:
    int value;
};

void tst_QAutoPtr::pointerOperator() const
{
    AnyForm af;
    af.value = 5;
    AutoPtr<AnyForm> p(&af);

    QCOMPARE(p->value, 5);
    p.release();
}

void tst_QAutoPtr::pointerOperatorSignature() const
{
    /* The operator should be const. */
    const AutoPtr<AnyForm> p(new AnyForm);
    p->value = 5;

    QVERIFY(p->value);
}

void tst_QAutoPtr::negationOperator() const
{
    /* Invoke on default constructed value. */
    {
        AutoPtr<int> p;
        QVERIFY(!p);
    }
}

void tst_QAutoPtr::negationOperatorSignature() const
{
    /* The signature should be const. */
    const AutoPtr<int> p;
    QVERIFY(!p);

    /* The return value should be bool. */
    QCOMPARE(!p, true);
}

void tst_QAutoPtr::operatorBool() const
{
    /* Invoke on default constructed value. */
    {
        AutoPtr<int> p;
        QCOMPARE(bool(p), false);
    }

    /* Invoke on active value. */
    {
        AutoPtr<int> p(new int(3));
        QVERIFY(p);
    }
}

void tst_QAutoPtr::operatorBoolSignature() const
{
    /* The signature should be const. */
    const AutoPtr<int> p;
    QCOMPARE(bool(p), false);
}

QTEST_MAIN(tst_QAutoPtr)

#include "tst_qautoptr.moc"
