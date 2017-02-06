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

#include <QFile>
#include <QtTest/QtTest>
#include <QLibraryInfo>

#include "../qxmlquery/TestFundament.h"

/*!
 \class tst_XmlPatterns
 \internal
 \since 4.6
 \brief Tests the command line interface, \c xmlpatternsvalidator, for the XML validation code.
 */
class tst_XmlPatternsValidator : public QObject
                               , private TestFundament
{
    Q_OBJECT

public:
    tst_XmlPatternsValidator();

private Q_SLOTS:
    void initTestCase();
    void xsdSupport();
    void xsdSupport_data() const;

private:
    const QString   m_command;
    bool            m_dontRun;
};

tst_XmlPatternsValidator::tst_XmlPatternsValidator()
    : m_command(QLibraryInfo::location(QLibraryInfo::BinariesPath) + QLatin1String("/xmlpatternsvalidator"))
    , m_dontRun(false)
{
}

void tst_XmlPatternsValidator::initTestCase()
{
#ifndef QT_NO_PROCESS
    QProcess process;
    process.start(m_command);

    if(!process.waitForFinished())
    {
        m_dontRun = true;
        QSKIP(
            qPrintable(QString(
                "The command line tool (%1) could not be run, possibly because Qt was "
                "incompletely built or installed. No tests will be run."
            ).arg(m_command))
        );
    }
#else
    QSKIP("Skipping test due to not having process support");
#endif // QT_NO_PROCESS
}

void tst_XmlPatternsValidator::xsdSupport()
{
    if(m_dontRun)
        QSKIP("The command line utility is not in the path.");

#ifndef QT_NO_PROCESS
    QFETCH(int,         expectedExitCode);
    QFETCH(QStringList, arguments);
    QFETCH(QString,     cwd);

    QProcess process;

    if(!cwd.isEmpty())
        process.setWorkingDirectory(inputFile(cwd));

    process.start(m_command, arguments);

    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);

    if(process.exitCode() != expectedExitCode)
        QTextStream(stderr) << "foo:" << process.readAllStandardError();

    QCOMPARE(process.exitCode(), expectedExitCode);
#else
    QSKIP("Skipping test due to not having process support");
#endif // QT_NO_PROCESS
}

void tst_XmlPatternsValidator::xsdSupport_data() const
{
    QString path = QFINDTESTDATA("files/");

    /* Check one file for existence, to avoid a flood of failures. */
    QVERIFY(QFile::exists(path + QLatin1String("instance.xml")));

    QTest::addColumn<int>("expectedExitCode");
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("cwd");

    QTest::newRow("No arguments")
        << 2
        << QStringList()
        << QString();

    QTest::newRow("A valid schema")
        << 0
        << QStringList(path + QLatin1String("valid_schema.xsd"))
        << QString();

    QTest::newRow("An invalid schema")
        << 1
        << QStringList(path + QLatin1String("invalid_schema.xsd"))
        << QString();

    QTest::newRow("An instance and valid schema")
        << 0
        << (QStringList() << path + QLatin1String("instance.xml")
                          << path + QLatin1String("valid_schema.xsd"))
        << QString();

    QTest::newRow("An instance and invalid schema")
        << 1
        << (QStringList() << path + QLatin1String("instance.xml")
                          << path + QLatin1String("invalid_schema.xsd"))
        << QString();

    QTest::newRow("An instance and not matching schema")
        << 1
        << (QStringList() << path + QLatin1String("instance.xml")
                          << path + QLatin1String("other_valid_schema.xsd"))
        << QString();

    QTest::newRow("Two instance documents")
        << 1
        << (QStringList() << path + QLatin1String("instance.xml")
                          << path + QLatin1String("instance.xml"))
        << QString();

    QTest::newRow("Three instance documents")
        << 2
        << (QStringList() << path + QLatin1String("instance.xml")
                          << path + QLatin1String("instance.xml")
                          << path + QLatin1String("instance.xml"))
        << QString();

    QTest::newRow("Two schema documents")
        << 1
        << (QStringList() << path + QLatin1String("valid_schema.xsd")
                          << path + QLatin1String("valid_schema.xsd"))
        << QString();

    QTest::newRow("A schema aware valid instance document")
        << 0
        << (QStringList() << path + QLatin1String("sa_valid_instance.xml"))
        << QString();

    QTest::newRow("A schema aware invalid instance document")
        << 1
        << (QStringList() << path + QLatin1String("sa_invalid_instance.xml"))
        << QString();

    QTest::newRow("A non-schema aware instance document")
        << 1
        << (QStringList() << path + QLatin1String("instance.xml"))
        << QString();

    QTest::newRow("QTBUG-8394 A schema with an indirectly included type")
        << 0
        << (QStringList() << path + QLatin1String("indirect-include-a.xsd"))
        << QString();

    QTest::newRow("QTBUG-8394 A schema with an indirectly imported type")
        << 0
        << (QStringList() << path + QLatin1String("indirect-import-a.xsd"))
        << QString();

    QTest::newRow("QTBUG-8394 A schema with an indirectly redefined type")
        << 0
        << (QStringList() << path + QLatin1String("indirect-redefine-a.xsd"))
        << QString();

    QTest::newRow("QTBUG-8920 A schema with a complex type that indirectly includes an anonymous type")
        << 0
        << (QStringList() << path + QLatin1String("complex-type-including-anonymous-type.xsd"))
        << QString();

    QTest::newRow("QTBUG-11559 A schema and instance with a dateTime containing microseconds")
        << 0
        << (QStringList() << path + QLatin1String("dateTime-with-microseconds.xml")
                          << path + QLatin1String("dateTime-with-microseconds.xsd"))
        << QString();

    QTest::newRow("A document with a valid substitution group")
        << 0
        << (QStringList() << path + QLatin1String("substitution-group-valid.xml")
                          << path + QLatin1String("substitution-group.xsd"))
        << QString();

    QTest::newRow("A document attempting to use a prohibited substitution")
        << 1
        << (QStringList() << path + QLatin1String("substitution-group-invalid.xml")
                          << path + QLatin1String("substitution-group.xsd"))
        << QString();
}

QTEST_MAIN(tst_XmlPatternsValidator)

#include "tst_xmlpatternsvalidator.moc"

// vim: et:ts=4:sw=4:sts=4
