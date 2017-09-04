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

#ifndef QLOCATIONTESTUTILS_P_H
#define QLOCATIONTESTUTILS_P_H

#include <QDebug>
#include <QString>
#include <QTest>

namespace QLocationTestUtils
{
    bool hasDefaultSource();
    bool hasDefaultMonitor();

    QString addNmeaChecksumAndBreaks(const QString &sentence);

    QString createRmcSentence(const QDateTime &dt);
    QString createGgaSentence(const QTime &time);
    QString createGgaSentence(int lat, int lng, const QTime &time);
    QString createZdaSentence(const QDateTime &dt);
    QString createGsaSentence();

    //The purpose of compareEquality() is to test equality
    //operators where it is expected that A == B.
    template<typename A, typename B>
    bool compareEquality(const A &first, const B &second) {
        if (first != second) {
            qWarning() << "compareEquality() failed: first != second";
            return false;
        }

        if (second != first) {
            qWarning() << "compareEquality() failed: second != first";
            return false;
        }

        if (!(first == second)) {
            qWarning() << "compareEquality() failed: !(first == second)";
            return false;
        }

        if (!(second == first)) {
            qWarning() << "compareEquality() failed: !(second == first)";
            return false;
        }

        return true;
    }

    //The purpose of compareInequality() is to test equality
    //operators where it is expected that A != B.
    //Using !compareEquality(...) is not sufficient because
    //only the first operator checked would end up being tested.
    template<typename A, typename B>
    bool compareInequality(const A &first, const B &second) {
        if (!(first != second)){
            qWarning() << "compareInequality() failed: !(first != second)";
            return false;
        }

        if (!(second != first)) {
            qWarning() << "compareInequality() failed: !(second != first)";
            return false;
        }

        if (first == second) {
            qWarning() << "compareInequality() failed: first == second)";
            return false;
        }

        if (second == first) {
            qWarning() << "compareInequality() failed: second == first";
            return false;
        }
        return true;
    }

    // Tests conversions between sub and base classes
    // TC (test case) must implement:
    // SubClass initialSubObject();
    // bool checkType(const BaseClass &)
    // void detach(BaseClass *) - calls a mutator method, but doesn't actually modify the
    //                            property to something different.
    // void setSubClassProperty(SubClass *) - sets a property in the subclass instance
    template<typename TC, typename BaseClass, typename SubClass>
    void testConversion(TC *tc) {
        SubClass sub = tc->initialSubObject();
        //check conversion from SubClass -> BaseClass
        //using assignment operator
        BaseClass base;
        base = sub;
        QVERIFY(QLocationTestUtils::compareEquality(base, sub));
        QVERIFY(tc->checkType(base));

        //check comparing base classes
        BaseClass base2;
        base2 = sub;
        QVERIFY(QLocationTestUtils::compareEquality(base, base2));

        //check conversion from BaseClass -> SubClass
        //using assignment operator
        SubClass sub2;
        sub2 = base;
        QVERIFY(QLocationTestUtils::compareEquality(sub, sub2));
        QVERIFY(tc->checkType(sub2));

        //check that equality still holds with detachment of underlying data pointer
        tc->detach(&base);
        sub2 = base;
        QVERIFY(QLocationTestUtils::compareEquality(sub, sub2));
        QVERIFY(QLocationTestUtils::compareEquality(sub, base));
        QVERIFY(QLocationTestUtils::compareEquality(base, base2));

        //check that comparing objects are not the same
        //when an underlying subclass field has been modified
        tc->setSubClassProperty(&sub2);
        base2 = sub2;
        QVERIFY(QLocationTestUtils::compareInequality(sub, sub2));
        QVERIFY(QLocationTestUtils::compareInequality(sub, base2));
        QVERIFY(QLocationTestUtils::compareInequality(base, base2));

        //check conversion from SubClass -> BaseClass
        //using copy constructor
        BaseClass base3(sub);
        QVERIFY(QLocationTestUtils::compareEquality(sub, base3));
        QVERIFY(QLocationTestUtils::compareEquality(base, base3));

        //check conversion from BaseClass -> SubClass
        //using copy constructor
        SubClass sub3(base3);
        QVERIFY(QLocationTestUtils::compareEquality(sub, sub3));

        //check conversion to subclass using a default base class instance
        BaseClass baseDefault;
        SubClass subDefault;
        SubClass sub4(baseDefault);
        QVERIFY(QLocationTestUtils::compareEquality(sub4, subDefault));

        SubClass sub5 = baseDefault;
        QVERIFY(QLocationTestUtils::compareEquality(sub5, subDefault));
    }
};

#endif
