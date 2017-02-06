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

#include <QtCore/QString>
#include <QtTest/QtTest>

#include <QtLocation/QPlaceContactDetail>

QT_USE_NAMESPACE

class tst_QPlaceContactDetail : public QObject
{
    Q_OBJECT

public:
    tst_QPlaceContactDetail();

private Q_SLOTS:
    void constructorTest();
    void labelTest();
    void valueTest();
    void clearTest();
    void operatorsTest();
    void operatorsTest_data();
};

tst_QPlaceContactDetail::tst_QPlaceContactDetail()
{
}

void tst_QPlaceContactDetail::constructorTest()
{
    QPlaceContactDetail detail;
    QVERIFY(detail.label().isEmpty());
    QVERIFY(detail.value().isEmpty());

    detail.setLabel(QStringLiteral("Emergency Services"));
    detail.setValue(QStringLiteral("0118 999"));

    QPlaceContactDetail detail2(detail);
    QCOMPARE(detail2.label(), QStringLiteral("Emergency Services"));
    QCOMPARE(detail2.value(), QStringLiteral("0118 999"));
}

void tst_QPlaceContactDetail::labelTest()
{
    QPlaceContactDetail detail;
    detail.setLabel(QStringLiteral("home"));
    QCOMPARE(detail.label(), QStringLiteral("home"));
    detail.setLabel(QString());
    QVERIFY(detail.label().isEmpty());
}

void tst_QPlaceContactDetail::valueTest()
{
    QPlaceContactDetail detail;
    detail.setValue(QStringLiteral("555-5555"));
    QCOMPARE(detail.value(), QStringLiteral("555-5555"));
    detail.setValue(QString());
    QVERIFY(detail.value().isEmpty());
}

void tst_QPlaceContactDetail::clearTest()
{
    QPlaceContactDetail detail;
    detail.setLabel(QStringLiteral("Ghostbusters"));
    detail.setValue(QStringLiteral("555-2368"));
    detail.clear();
    QVERIFY(detail.label().isEmpty());
    QVERIFY(detail.value().isEmpty());
}

void tst_QPlaceContactDetail::operatorsTest()
{
    QPlaceContactDetail detail1;
    detail1.setLabel(QStringLiteral("Kramer"));
    detail1.setValue(QStringLiteral("555-filk"));

    QPlaceContactDetail detail2;
    detail2.setLabel(QStringLiteral("Kramer"));
    detail2.setValue(QStringLiteral("555-filk"));

    QVERIFY(detail1 == detail2);
    QVERIFY(!(detail1 != detail2));
    QVERIFY(detail2 == detail1);
    QVERIFY(!(detail2 != detail1));

    QPlaceContactDetail detail3;
    QVERIFY(!(detail1 == detail3));
    QVERIFY(detail1 != detail3);
    QVERIFY(!(detail1 == detail3));
    QVERIFY(detail1 != detail3);

    detail3 = detail1;
    QVERIFY(detail1 == detail3);
    QVERIFY(!(detail1 != detail3));
    QVERIFY(detail3 == detail1);
    QVERIFY(!(detail3 != detail1));

    QFETCH(QString, field);
    if (field == QStringLiteral("label"))
        detail3.setLabel(QStringLiteral("Cosmo"));
    else if (field == QStringLiteral("value"))
        detail3.setValue(QStringLiteral("555-5555"));

    QVERIFY(!(detail1 == detail3));
    QVERIFY(detail1 != detail3);
    QVERIFY(!(detail3 == detail1));
    QVERIFY(detail3 != detail1);
}

void tst_QPlaceContactDetail::operatorsTest_data()
{
    QTest::addColumn<QString>("field");
    QTest::newRow("label") << "label";
    QTest::newRow("value") << "value";
}

QTEST_APPLESS_MAIN(tst_QPlaceContactDetail)

#include "tst_qplacecontactdetail.moc"
