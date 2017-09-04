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

#include <QtXmlPatterns/QXmlNamePool>
#include <QtXmlPatterns/QXmlName>

/*!
 \class tst_QXmlName
 \internal
 \since 4.4
 \brief Tests class QXmlName.

 This test is not intended for testing the engine, but the functionality specific
 to the QXmlName class.

 In other words, if you have an engine bug; don't add it here because it won't be
 tested properly. Instead add it to the test suite.

 */
class tst_QXmlName : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultConstructor() const;
    void argumentConstructor() const;
    void argumentConstructor_data() const;
    void argumentConstructorDefaultArguments() const;
    void equalnessoperator() const;
    void inequalnessoperator() const;

    void isNull() const;
    void operatorEqual() const;
    void operatorEqual_data() const;
    void operatorNotEqual() const;
    void operatorNotEqual_data() const;
    void toClarkName() const;
    void toClarkName_data() const;
    void constCorrectness() const;
    void qHash() const;
    void objectSize() const;
    void withinQVariant() const;
    void typeWithinQVariant() const;
    void isNCName() const;
    void isNCName_data() const;
    void isNCNameSignature() const;
    void fromClarkName() const;
    void fromClarkName_data() const;
    void fromClarkNameSignature() const;
};

void tst_QXmlName::defaultConstructor() const
{
    /* Allocate instance in different orders. */
    {
        QXmlName name;
    }

    {
        QXmlName name1;
        QXmlName name2;
        QXmlName name3;
    }
}

Q_DECLARE_METATYPE(QXmlNamePool)
void tst_QXmlName::argumentConstructor() const
{
    QFETCH(QString, namespaceURI);
    QFETCH(QString, localName);
    QFETCH(QString, prefix);
    QFETCH(QXmlNamePool, namePool);

    const QXmlName name(namePool, localName, namespaceURI, prefix);

    QCOMPARE(name.namespaceUri(namePool), namespaceURI);
    QCOMPARE(name.localName(namePool), localName);
    QCOMPARE(name.prefix(namePool), prefix);
}

/*!
  \internal

 Below we use the same QXmlNamePool instance. This means the same name pool
 is used.
 */
void tst_QXmlName::argumentConstructor_data() const
{
    QTest::addColumn<QString>("namespaceURI");
    QTest::addColumn<QString>("localName");
    QTest::addColumn<QString>("prefix");
    QTest::addColumn<QXmlNamePool>("namePool");

    QXmlNamePool namePool;
    QTest::newRow("Basic test")
                    << QString::fromLatin1("http://example.com/Namespace1")
                    << QString::fromLatin1("localName1")
                    << QString::fromLatin1("prefix1")
                    << namePool;

    QTest::newRow("Same namespace & prefix as before, different local name.")
                    << QString::fromLatin1("http://example.com/Namespace1")
                    << QString::fromLatin1("localName2")
                    << QString::fromLatin1("prefix1")
                    << namePool;

    QTest::newRow("Same namespace & local name as before, different prefix.")
                    << QString::fromLatin1("http://example.com/Namespace1")
                    << QString::fromLatin1("localName2")
                    << QString::fromLatin1("prefix2")
                    << namePool;

    QTest::newRow("No prefix")
                    << QString::fromLatin1("http://example.com/Namespace2")
                    << QString::fromLatin1("localName3")
                    << QString()
                    << namePool;
}

/*!
 Ensure that the three last arguments have default values, and that they are null strings.
 */
void tst_QXmlName::argumentConstructorDefaultArguments() const
{
    QXmlNamePool np;
    const QXmlName n1(np, QLatin1String("localName"));
    const QXmlName n2(np, QLatin1String("localName"), QString(), QString());

    QCOMPARE(n1, n2);
    QCOMPARE(n1.toClarkName(np), QString::fromLatin1("localName"));
}

void tst_QXmlName::equalnessoperator() const
{
    const QXmlName o1;
    const QXmlName o2;
    o1 == o2;
    // TODO
}

void tst_QXmlName::inequalnessoperator() const
{
    const QXmlName o1;
    const QXmlName o2;
    o1 != o2;
    // TODO
}

void tst_QXmlName::isNull() const
{
    /* Check default value. */
    QXmlName name;
    QVERIFY(name.isNull());
}

void tst_QXmlName::operatorEqual() const
{
    QFETCH(QXmlName, op1);
    QFETCH(QXmlName, op2);
    QFETCH(bool, expected);

    QCOMPARE(op1 == op2, expected);
}

void tst_QXmlName::operatorEqual_data() const
{
    QTest::addColumn<QXmlName>("op1");
    QTest::addColumn<QXmlName>("op2");
    QTest::addColumn<bool>("expected");

    QXmlNamePool namePool;
    const QXmlName n1(namePool, QString::fromLatin1("localName1"),
                                QString::fromLatin1("http://example.com/Namespace1"),
                                QString::fromLatin1("prefix1"));

    const QXmlName n2(namePool, QString::fromLatin1("localName2"),
                                QString::fromLatin1("http://example.com/Namespace1"),
                                QString::fromLatin1("prefix1"));

    const QXmlName n3(namePool, QString::fromLatin1("localName2"),
                                QString::fromLatin1("http://example.com/Namespace1"),
                                QString::fromLatin1("prefix2"));

    const QXmlName n4(namePool, QString::fromLatin1("localName3"),
                                QString::fromLatin1("http://example.com/Namespace2"));

    const QXmlName n5(namePool, QString::fromLatin1("localName4"),
                                QString::fromLatin1("http://example.com/Namespace2"));

    const QXmlName n6(namePool, QString::fromLatin1("localName4"),
                                QString::fromLatin1("http://example.com/Namespace2"),
                                QString::fromLatin1("prefix3"));

    const QXmlName n7(namePool, QString::fromLatin1("localName2"),
                                QString::fromLatin1("http://example.com/Namespace2"),
                                QString::fromLatin1("prefix3"));

    QTest::newRow(qPrintable(n1.toClarkName(namePool)))
        << n1
        << n1
        << true;

    QTest::newRow(qPrintable(n2.toClarkName(namePool)))
        << n2
        << n2
        << true;

    QTest::newRow(qPrintable(n3.toClarkName(namePool)))
        << n3
        << n3
        << true;

    QTest::newRow(qPrintable(n4.toClarkName(namePool)))
        << n4
        << n4
        << true;

    QTest::newRow(qPrintable(n5.toClarkName(namePool)))
        << n5
        << n5
        << true;

    QTest::newRow(qPrintable(n6.toClarkName(namePool)))
        << n6
        << n6
        << true;

    QTest::newRow(qPrintable(n7.toClarkName(namePool)))
        << n7
        << n7
        << true;

    QTest::newRow("Prefix differs")
        << n2
        << n3
        << true;

    QTest::newRow("No prefix vs. prefix")
        << n5
        << n6
        << true;

    QTest::newRow("Local name differs")
        << n1
        << n2
        << false;

    QTest::newRow("Namespace differs")
        << n2
        << n7
        << false;
}

void tst_QXmlName::operatorNotEqual() const
{
    QFETCH(QXmlName, op1);
    QFETCH(QXmlName, op2);
    QFETCH(bool, expected);

    QCOMPARE(op1 != op2, !expected);
}

void tst_QXmlName::operatorNotEqual_data() const
{
    operatorEqual_data();
}

/*!
  Check that functions have the correct const qualification.
 */
void tst_QXmlName::constCorrectness() const
{
    const QXmlName name;

    /* isNull() */
    QVERIFY(name.isNull());

    /* operator==() */
    QVERIFY(name == name);

    /* operator!=() */
    QVERIFY(!(name != name));

    QXmlNamePool namePool;
    const QXmlName name2(namePool, QLatin1String("localName"), QLatin1String("http://example.com/"), QLatin1String("prefix"));

    /* namespaceUri(). */
    QCOMPARE(name2.namespaceUri(namePool), QLatin1String("http://example.com/"));

    /* localName(). */
    QCOMPARE(name2.localName(namePool), QLatin1String("localName"));

    /* prefix(). */
    QCOMPARE(name2.prefix(namePool), QLatin1String("prefix"));

    /* toClarkname(). */
    QCOMPARE(name2.toClarkName(namePool), QLatin1String("{http://example.com/}prefix:localName"));
}

void tst_QXmlName::qHash() const
{
    /* Just call it, so we know it exist and that we don't trigger undefined
     * behavior. We can't test the return value, since it's opaque. */
    QXmlName name;
    ::qHash(name);
}

void tst_QXmlName::objectSize() const
{
    QVERIFY2(sizeof(QXmlName) == sizeof(qint64), "QXmlName should have at least a d-pointer.");
}

void tst_QXmlName::toClarkName() const
{
    QFETCH(QString, produced);
    QFETCH(QString, expected);

    QCOMPARE(produced, expected);
}

void tst_QXmlName::toClarkName_data() const
{
    QTest::addColumn<QString>("produced");
    QTest::addColumn<QString>("expected");

    QXmlNamePool np;

    /* A null QXmlName. */
    {
        const QXmlName n;
        QTest::newRow("") << n.toClarkName(np)
                          << QString::fromLatin1("QXmlName(null)");
    }

    {
        const QXmlName n(np, QLatin1String("localName"));
        QTest::newRow("") << n.toClarkName(np)
                          << QString::fromLatin1("localName");
    }

    /* Local name with namespace URI, empty prefix. */
    {
        const QXmlName n(np, QLatin1String("localName"),
                             QLatin1String("http://example.com/"));
        QTest::newRow("") << n.toClarkName(np)
                          << QString::fromLatin1("{http://example.com/}localName");
    }

    /* Local name with namespace URI and prefix. */
    {
        const QXmlName n(np, QLatin1String("localName"),
                             QLatin1String("http://example.com/"),
                             QLatin1String("p"));
        QTest::newRow("") << n.toClarkName(np)
                          << QString::fromLatin1("{http://example.com/}p:localName");
    }
}

/*!
  Check that QXmlName can be used inside QVariant.
 */
void tst_QXmlName::withinQVariant() const
{
    /* The extra paranthesis silences a warning on win32-msvc2005. */
    QVariant value(qVariantFromValue(QXmlName()));
}

/*!
 Check that the user type of QXmlName holds.
 */
void tst_QXmlName::typeWithinQVariant() const
{
    const int qxmlNameType = QVariant(qVariantFromValue(QXmlName())).userType();

    const QVariant value(qVariantFromValue(QXmlName()));

    QCOMPARE(value.userType(), qxmlNameType);
}

/*!
  We don't do full testing here. Don't have the resources for it. We simply assume
  we use a code path which is fully tested elsewhere.
 */
void tst_QXmlName::isNCName() const
{
    QFETCH(QString, input);
    QFETCH(bool, expectedValidity);

    QCOMPARE(QXmlName::isNCName(input), expectedValidity);
}

void tst_QXmlName::isNCName_data() const
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<bool>("expectedValidity");

    QTest::newRow("empty string")
        << QString()
        << false;

    QTest::newRow("A number")
        << QString::fromLatin1("1")
        << false;

    QTest::newRow("Simple valid string")
        << QString::fromLatin1("abc")
        << true;

    QTest::newRow("Simple valid string")
        << QString::fromLatin1("abc.123")
        << true;
}

void tst_QXmlName::isNCNameSignature() const
{
    const QString constQString;

    /* Verify that we can take a const QString. */
    QXmlName::isNCName(constQString);

    /* Verify that we can take a temporary QString. */
    QXmlName::isNCName(QString());
}

void tst_QXmlName::fromClarkName() const
{
    QFETCH(QString,         input);
    QFETCH(QXmlName,        expected);
    QFETCH(QXmlNamePool,    namePool);

    QCOMPARE(QXmlName::fromClarkName(input, namePool), expected);
}

void tst_QXmlName::fromClarkName_data() const
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QXmlName>("expected");
    QTest::addColumn<QXmlNamePool>("namePool");

    QXmlNamePool np;

    QTest::newRow("A null string")
        << QString()
        << QXmlName()
        << np;

    QTest::newRow("An empty string")
        << QString(QLatin1String(""))
        << QXmlName()
        << np;

    QTest::newRow("A single local name")
        << QString(QLatin1String("foo"))
        << QXmlName(np, QLatin1String("foo"))
        << np;

    QTest::newRow("Has prefix, but no namespace, that's invalid")
        << QString(QLatin1String("prefix:foo"))
        << QXmlName()
        << np;

    QTest::newRow("Namespace, local name, no prefix")
        << QString(QLatin1String("{def}abc"))
        << QXmlName(np, QLatin1String("abc"), QLatin1String("def"))
        << np;

    QTest::newRow("Namespace, local name, prefix")
        << QString(QLatin1String("{def}p:abc"))
        << QXmlName(np, QLatin1String("abc"), QLatin1String("def"), QLatin1String("p"))
        << np;

    QTest::newRow("Namespace, local name, prefix syntax error")
        << QString(QLatin1String("{def}:abc"))
        << QXmlName()
        << np;

    QTest::newRow("Namespace, local name syntax error, prefix")
        << QString(QLatin1String("{def}p:"))
        << QXmlName()
        << np;

    QTest::newRow("Only local name which is invalid")
        << QString(QLatin1String(":::"))
        << QXmlName()
        << np;

    QTest::newRow("Namespace, invalid local name")
        << QString(QLatin1String("{def}a|bc"))
        << QXmlName()
        << np;

    QTest::newRow("Namespace, local name, invalid prefix")
        << QString(QLatin1String("{def}a|b:c"))
        << QXmlName()
        << np;

    QTest::newRow("A single left curly, invalid")
        << QString(QLatin1String("{"))
        << QXmlName()
        << np;

    QTest::newRow("A single left curly, invalid")
        << QString(QLatin1String("{aaswd"))
        << QXmlName()
        << np;
}

void tst_QXmlName::fromClarkNameSignature() const
{
    /* We should take const references. */
    const QXmlNamePool np;
    const QString in;

    QXmlName::fromClarkName(in, np);
}

QTEST_MAIN(tst_QXmlName)

#include "tst_qxmlname.moc"

// vim: et:ts=4:sw=4:sts=4
