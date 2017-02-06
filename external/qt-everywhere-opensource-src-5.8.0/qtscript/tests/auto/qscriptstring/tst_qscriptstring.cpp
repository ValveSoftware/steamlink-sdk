/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptstring.h>

class tst_QScriptString : public QObject
{
    Q_OBJECT

public:
    tst_QScriptString();
    virtual ~tst_QScriptString();

private slots:
    void test();
    void hash();
    void toArrayIndex_data();
    void toArrayIndex();
};

tst_QScriptString::tst_QScriptString()
{
}

tst_QScriptString::~tst_QScriptString()
{
}

void tst_QScriptString::test()
{
    QScriptEngine eng;

    {
        QScriptString str;
        QVERIFY(!str.isValid());
        QVERIFY(str == str);
        QVERIFY(!(str != str));
        QVERIFY(str.toString().isNull());

        QScriptString str1(str);
        QVERIFY(!str1.isValid());

        QScriptString str2 = str;
        QVERIFY(!str2.isValid());

        QCOMPARE(str.toArrayIndex(), quint32(0xffffffff));
    }

    for (int x = 0; x < 2; ++x) {
        QString ciao = QString::fromLatin1("ciao");
        QScriptString str = eng.toStringHandle(ciao);
        QVERIFY(str.isValid());
        QVERIFY(str == str);
        QVERIFY(!(str != str));
        QCOMPARE(str.toString(), ciao);

        QScriptString str1(str);
        QCOMPARE(str, str1);

        QScriptString str2 = str;
        QCOMPARE(str, str2);

        QScriptString str3 = eng.toStringHandle(ciao);
        QVERIFY(str3.isValid());
        QCOMPARE(str, str3);

        eng.collectGarbage();

        QVERIFY(str.isValid());
        QCOMPARE(str.toString(), ciao);
        QVERIFY(str1.isValid());
        QCOMPARE(str1.toString(), ciao);
        QVERIFY(str2.isValid());
        QCOMPARE(str2.toString(), ciao);
        QVERIFY(str3.isValid());
        QCOMPARE(str3.toString(), ciao);
    }

    {
        QScriptEngine *eng2 = new QScriptEngine;
        QString one = QString::fromLatin1("one");
        QString two = QString::fromLatin1("two");
        QScriptString oneInterned = eng2->toStringHandle(one);
        QCOMPARE(oneInterned.toString(), one);
        QScriptString twoInterned = eng2->toStringHandle(two);
        QCOMPARE(twoInterned.toString(), two);
        QVERIFY(oneInterned != twoInterned);
        QVERIFY(!(oneInterned == twoInterned));
        QScriptString copy1(oneInterned);
        QScriptString copy2 = oneInterned;

        delete eng2;

        QVERIFY(!oneInterned.isValid());
        QVERIFY(!twoInterned.isValid());
        QVERIFY(!copy1.isValid());
        QVERIFY(!copy2.isValid());
    }
}

void tst_QScriptString::hash()
{
    QScriptEngine engine;
    QHash<QScriptString, int> stringToInt;
    QScriptString foo = engine.toStringHandle("foo");
    QScriptString bar = engine.toStringHandle("bar");
    QVERIFY(!stringToInt.contains(foo));
    for (int i = 0; i < 1000000; ++i)
        stringToInt.insert(foo, 123);
    QCOMPARE(stringToInt.value(foo), 123);
    QVERIFY(!stringToInt.contains(bar));
    stringToInt.insert(bar, 456);
    QCOMPARE(stringToInt.value(bar), 456);
    QCOMPARE(stringToInt.value(foo), 123);
}

void tst_QScriptString::toArrayIndex_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectSuccess");
    QTest::addColumn<quint32>("expectedIndex");
    QTest::newRow("foo") << QString::fromLatin1("foo") << false << quint32(0xffffffff);
    QTest::newRow("empty") << QString::fromLatin1("") << false << quint32(0xffffffff);
    QTest::newRow("0") << QString::fromLatin1("0") << true << quint32(0);
    QTest::newRow("00") << QString::fromLatin1("00") << false << quint32(0xffffffff);
    QTest::newRow("1") << QString::fromLatin1("1") << true << quint32(1);
    QTest::newRow("123") << QString::fromLatin1("123") << true << quint32(123);
    QTest::newRow("-1") << QString::fromLatin1("-1") << false << quint32(0xffffffff);
    QTest::newRow("0a") << QString::fromLatin1("0a") << false << quint32(0xffffffff);
    QTest::newRow("0x1") << QString::fromLatin1("0x1") << false << quint32(0xffffffff);
    QTest::newRow("01") << QString::fromLatin1("01") << false << quint32(0xffffffff);
    QTest::newRow("101a") << QString::fromLatin1("101a") << false << quint32(0xffffffff);
    QTest::newRow("4294967294") << QString::fromLatin1("4294967294") << true << quint32(0xfffffffe);
    QTest::newRow("4294967295") << QString::fromLatin1("4294967295") << false << quint32(0xffffffff);
    QTest::newRow("0.0") << QString::fromLatin1("0.0") << false << quint32(0xffffffff);
    QTest::newRow("1.0") << QString::fromLatin1("1.0") << false << quint32(0xffffffff);
    QTest::newRow("1.5") << QString::fromLatin1("1.5") << false << quint32(0xffffffff);
    QTest::newRow("1.") << QString::fromLatin1("1.") << false << quint32(0xffffffff);
    QTest::newRow(".1") << QString::fromLatin1(".1") << false << quint32(0xffffffff);
    QTest::newRow("1e0") << QString::fromLatin1("1e0") << false << quint32(0xffffffff);
}

void tst_QScriptString::toArrayIndex()
{
    QFETCH(QString, input);
    QFETCH(bool, expectSuccess);
    QFETCH(quint32, expectedIndex);
    QScriptEngine engine;
    for (int x = 0; x < 2; ++x) {
        bool isArrayIndex;
        bool *ptr = (x == 0) ? &isArrayIndex : (bool*)0;
        quint32 result = engine.toStringHandle(input).toArrayIndex(ptr);
        if (x == 0)
            QCOMPARE(isArrayIndex, expectSuccess);
        QCOMPARE(result, expectedIndex);
    }
}

QTEST_MAIN(tst_QScriptString)
#include "tst_qscriptstring.moc"
