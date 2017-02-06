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


#include <QFile>
#include <QtTest/QtTest>

#include "../qxmlquery/TestFundament.h"
#include "../network-settings.h"

/*!
 \class tst_XmlPatterns
 \internal
 \since 4.4
 \brief Tests the command line interface, \c xmlpatterns, for the XQuery code.

 This test is not intended for testing the engine, but all the gluing the
 command line interface do: error reporting, query output, variable bindings, exit
 codes, and so on.

 In other words, if you have an engine bug; don't add it here because it won't be
 tested properly. Instead add it to the test suite.

 */
class tst_XmlPatterns : public QObject
                      , private TestFundament
{
    Q_OBJECT

public:
    tst_XmlPatterns();

private Q_SLOTS:
    void initTestCase();
    void xquerySupport();
    void xquerySupport_data() const;
    void xsltSupport();
    void xsltSupport_data() const;
    void stdoutFailure() const;
    void cleanupTestCase() const;

private:
    static void createNonWritable(const QString &name);
    static void removeNonWritable(QFile &outFile);

    QString filterStderr(const QString &in);

    int             m_generatedTests;
    /**
     * Get rid of characters that complicates on various file systems.
     */
    const QRegExp   m_normalizeTestName;
    /**
     * @note Perforce disallows wildcards in the name.
     */
    const QString   m_command;
    bool            m_dontRun;
};

tst_XmlPatterns::tst_XmlPatterns() : m_generatedTests(0)
                                   , m_normalizeTestName(QLatin1String("[\\*\\?#\\-\\/:; ()',&]"))
                                   , m_command(QLibraryInfo::location(QLibraryInfo::BinariesPath) + QLatin1String("/xmlpatterns"))
                                   , m_dontRun(false)
{
}

void tst_XmlPatterns::initTestCase()
{
    QVERIFY(m_normalizeTestName.isValid());

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

#ifndef QT_NO_PROCESS
static QByteArray msgProcessError(const char *what, const QProcess &process)
{
    QString result = QLatin1String(what) + QLatin1Char(' ')
        + QDir::toNativeSeparators(process.program())
        + QLatin1Char(' ') + process.arguments().join(QLatin1Char(' '))
        + QLatin1String(": ") + process.errorString();
    return result.toLocal8Bit();
}
#endif // !QT_NO_PROCESS

void tst_XmlPatterns::xquerySupport()
{
    if (QTest::currentDataTag() == QByteArray("Load query via FTP")
            || QTest::currentDataTag() == QByteArray("Load query via HTTP"))
        QVERIFY(QtNetworkSettings::verifyTestNetworkSettings());

    if(m_dontRun)
        QSKIP("The command line utility is not in the path.");

#ifndef QT_NO_PROCESS
    QFETCH(int,         expectedExitCode);
    QFETCH(QByteArray,  expectedQueryOutput);
    QFETCH(QStringList, arguments);
    QFETCH(QString,     cwd);
    QFETCH(QString,     outputFile);

    QProcess process;

    if(!cwd.isEmpty())
        process.setWorkingDirectory(inputFile(cwd));

    QProcessEnvironment env(process.processEnvironment());
    env.insert("QT_LOGGING_RULES", "qt.network.ssl=false");
    process.setProcessEnvironment(env);

    process.start(m_command, arguments);
    QVERIFY2(process.waitForStarted(), msgProcessError("Failed to start", process).constData());

    QVERIFY2(process.waitForFinished(), msgProcessError("Timeout running", process).constData());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);

    if(process.exitCode() != expectedExitCode)
        QTextStream(stderr) << "stderr:" << process.readAllStandardError();

    QCOMPARE(process.exitCode(), expectedExitCode);

    const QByteArray rawProducedStderr((process.readAllStandardError()));
    QString fixedStderr = filterStderr(QString::fromLocal8Bit(rawProducedStderr));
    // convert Windows line endings to Unix ones
    fixedStderr.replace("\r\n", "\n");

    const QString errorFileName(QFINDTESTDATA("stderrBaselines/") +
                                QString::fromUtf8(QTest::currentDataTag()).remove(m_normalizeTestName) +
                                QLatin1String(".txt"));

    QFile writeErr(errorFileName);

    if(writeErr.exists())
    {
        QVERIFY(writeErr.open(QIODevice::ReadOnly));
        QString rawExpectedStdErr(QString::fromLocal8Bit(writeErr.readAll()));

        /* On Windows, at least MinGW, this differs. */
        if(qstrcmp(QTest::currentDataTag(), "-output with a non-writable file") == 0)
        {
            QVERIFY(fixedStderr == filterStderr(rawExpectedStdErr) ||
                    fixedStderr.trimmed() == "Failed to open file notWritable.out for writing: Access is denied.");
        }
        else if(qstrcmp(QTest::currentDataTag(), "Invoke -version") == 0)
        {
            /* There's a wide range of different version strings used. For
             * instance, "4.4.0-rc1". */
            QRegExp removeVersion(QLatin1String(" Qt \\d\\.\\d.*"));
            QVERIFY(removeVersion.isValid());
            QCOMPARE(QString(fixedStderr).remove(removeVersion) + QChar('|'), rawExpectedStdErr + QChar('|'));
        }
        else
            QCOMPARE(fixedStderr, filterStderr(rawExpectedStdErr));
    }
    else
    {
        QFile writeErr(errorFileName);
        QVERIFY(writeErr.open(QIODevice::WriteOnly));

        QCOMPARE(writeErr.write(rawProducedStderr), qint64(rawProducedStderr.count()));
        QTextStream(stderr) << "creating file " << errorFileName;
        ++m_generatedTests;
    }

    const QByteArray actual(process.readAllStandardOutput());

    if(outputFile.isEmpty())
    {
        QCOMPARE(actual, expectedQueryOutput);
        return; /* We're done, this test was not creating any output file. */
    }
    else
    {
        QVERIFY(actual.isEmpty());

        QFile outFile(outputFile);

        QVERIFY(outFile.exists());
        QVERIFY(outFile.open(QIODevice::ReadOnly));

        QCOMPARE(outFile.readAll(), expectedQueryOutput);

        removeNonWritable(outFile);
    }

#else
    QSKIP("Skipping test due to not having process support");
#endif // QT_NO_PROCESS
}

void tst_XmlPatterns::xquerySupport_data() const
{
    QString path = QFINDTESTDATA("queries/");

    /* Check one file for existence, to avoid possible false positives. */
    QVERIFY(QFile::exists(path + QLatin1String("onePlusOne.xq")));

    QTest::addColumn<int>("expectedExitCode");
    QTest::addColumn<QByteArray>("expectedQueryOutput");
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("cwd");
    QTest::addColumn<QString>("outputFile");

    QTest::newRow("A simple math query")
        << 0
        << QByteArray("2\n")
        << QStringList((path + QLatin1String("onePlusOne.xq")))
        << QString()
        << QString();

    QTest::newRow("An unbound external variable")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("externalVariable.xq"))
        << QString()
        << QString();

    QTest::newRow("Bind an external variable")
        << 0
        << QByteArray("1 4<e>1</e>true\n")
        << (QStringList() << path + QLatin1String("externalVariable.xq")
                          << QLatin1String("-param")
                          << QLatin1String("externalVariable=1"))
        << QString()
        << QString();

    QTest::newRow("Bind an external variable, query appearing last")
        << 0
        << QByteArray("1 4<e>1</e>true\n")
        << (QStringList() << QLatin1String("-param")
                          << QLatin1String("externalVariable=1")
                          << path + QLatin1String("externalVariable.xq"))
        << QString()
        << QString();

    QTest::newRow("Use fn:doc")
        << 0
        << QByteArray("<e xmlns=\"http://example.com\" xmlns:p=\"http://example.com/P\" attr=\"1\" p:attr=\"\">\n    <?target data?>\n    <!-- a comment -->\n    <e/>text <f/>text node</e>\n")
        << QStringList(path + QLatin1String("openDocument.xq"))
        << QString()
        << QString();

    QTest::newRow("Use fn:doc, together with -no-format, last")
        << 0
        << QByteArray("<e xmlns=\"http://example.com\" xmlns:p=\"http://example.com/P\" attr=\"1\" p:attr=\"\"><?target data?><!-- a comment --><e/>text <f/>text node</e>")
        << (QStringList() << path + QLatin1String("openDocument.xq")
                          << QLatin1String("-no-format"))
        << QString()
        << QString();

    QTest::newRow("Use fn:doc, together with -no-format, first")
        << 0
        << QByteArray("<e xmlns=\"http://example.com\" xmlns:p=\"http://example.com/P\" attr=\"1\" p:attr=\"\"><?target data?><!-- a comment --><e/>text <f/>text node</e>")
        << (QStringList() << QLatin1String("-no-format")
                          << path + QLatin1String("openDocument.xq"))
        << QString()
        << QString();

    /* This is true for the command line utility, but not QXmlQuery::setQuery(). */
    QTest::newRow("Make sure query paths are resolved against CWD, not the location of the executable.")
        << 0
        << QByteArray("2\n")
        << QStringList(QFINDTESTDATA("queries/onePlusOne.xq"))
        << QFINDTESTDATA("queries")
        << QString();

    QTest::newRow("Call fn:error()")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("errorFunction.xq"))
        << QString()
        << QString();

    QTest::newRow("Evaluate a library module")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("simpleLibraryModule.xq"))
        << QString()
        << QString();

    QTest::newRow("Trigger a static error.")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("staticError.xq"))
        << QString()
        << QString();

    QTest::newRow("Pass -help")
        << 0
        << QByteArray()
        << QStringList(QLatin1String("-help"))
        << QString()
        << QString();

    QTest::newRow("Open an nonexistent file")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("ThisFileDoesNotExist.xq"))
        << QString()
        << QString();

    /* The following five tests exists to test the various
     * markup classes in the message. */
    QTest::newRow("XQuery-function message markups")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("wrongArity.xq"))
        << QString()
        << QString();

    QTest::newRow("XQuery-type message markups")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("typeError.xq"))
        << QString()
        << QString();

    QTest::newRow("XQuery-data & XQuery-keyword message markups")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("zeroDivision.xq"))
        << QString()
        << QString();

    QTest::newRow("XQuery-uri message markups")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("unsupportedCollation.xq"))
        << QString()
        << QString();

    QTest::newRow("XQuery-expression message markups")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("invalidRegexp.xq"))
        << QString()
        << QString();

    QTest::newRow("Print a list of available regexp flags(The available flags are formatted in a complex way.)")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("invalidRegexpFlag.xq"))
        << QString()
        << QString();

    QTest::newRow("Trigger an assert in QPatternist::ColorOutput. The query naturally contains an error; XPTY0004.")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("flwor.xq"))
        << QString()
        << QString();

    QTest::newRow("Trigger a second assert in QPatternist::ColorOutput. The query naturally contains XPST0003.")
        << 2
        << QByteArray()
        << QStringList(path + QLatin1String("syntaxError.xq"))
        << QString()
        << QString();

    QTest::newRow("-param is missing so multiple queries appear")
        << 2
        << QByteArray()
        << (QStringList() << path + QLatin1String("reportGlobals.xq")
                          << QLatin1String("fileToOpen=globals.gccxml"))
        << QString()
        << QString();

    QTest::newRow("only -no-format")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-no-format"))
        << QString()
        << QString();

    QTest::newRow("Basic use of -output, query first")
        << 0
        << QByteArray("2\n")
        << (QStringList() << path + QLatin1String("onePlusOne.xq")
                          << QLatin1String("-output")
                          << QLatin1String("basicOutput.out"))
        << QString()
        << QString::fromLatin1("basicOutput.out");

    QTest::newRow("Basic use of -output, query last")
        << 0
        << QByteArray("<e/>\n")
        << (QStringList() << QLatin1String("-output")
                          << QLatin1String("basicOutput2.out")
                          << path + QLatin1String("oneElement.xq"))
        << QString()
        << QString::fromLatin1("basicOutput2.out");

    QTest::newRow("A single query, that does not exist")
        << 2
        << QByteArray()
        << (QStringList() << path + QLatin1String("doesNotExist.xq"))
        << QString()
        << QString();

    QTest::newRow("Specify two identical query names")
        << 2
        << QByteArray()
        << (QStringList() << path + QLatin1String("query.xq")
                          << path + QLatin1String("query.xq"))
        << QString()
        << QString();

    QTest::newRow("Specify no arguments at all.")
        << 1
        << QByteArray()
        << QStringList()
        << QString()
        << QString();

    QTest::newRow("Use -output twice")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-output")
                          << QLatin1String("output1")
                          << QLatin1String("-output")
                          << QLatin1String("output2"))
        << QString()
        << QString();

    {
        const QString filename(QString::fromLatin1("notWritable.out"));
        createNonWritable(filename);

        QTest::newRow("-output with a non-writable file")
            << 1
            << QByteArray()
            << (QStringList() << QLatin1String("-output")
                              << filename
                              << path + QLatin1String("onePlusOne.xq"))
            << QString()
            << filename;
    }

    {
        const QString outName(QString::fromLatin1("existingContent.out"));
        QFile outFile(outName);
        QVERIFY(outFile.open(QIODevice::WriteOnly));
        QCOMPARE(outFile.write("Existing content\n"), qint64(17));
        outFile.close();

        QTest::newRow("Use -output on a file with existing content, to ensure we truncate, not append the content we produce.")
            << 0
            << QByteArray("2\n")
            << (QStringList() << QLatin1String("-output")
                              << outName
                              << path + QLatin1String("onePlusOne.xq"))
            << QString()
            << outName;
    }

    QTest::newRow("one query, and a terminating dash at the end")
        << 0
        << QByteArray("2\n")
        << (QStringList() << path + QLatin1String("onePlusOne.xq")
                          << QLatin1String("-"))
        << QString()
        << QString();

    QTest::newRow("one query, with a preceding dash")
        << 0
        << QByteArray("2\n")
        << (QStringList() << QLatin1String("-")
                          << path + QLatin1String("onePlusOne.xq"))
        << QString()
        << QString();

    QTest::newRow("A single dash, that's invalid")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-"))
        << QString()
        << QString();

    QTest::newRow("Invoke -version")
        << 0
        << QByteArray()
        << (QStringList() << QLatin1String("-version"))
        << QString()
        << QString();

    QTest::newRow("Unknown switch; -unknown-switch")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-unknown-switch"))
        << QString()
        << QString();

    QTest::newRow("Unknown switch; -d")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-d"))
        << QString()
        << QString();

    QTest::newRow("Passing a single dash is insufficient")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-"))
        << QString()
        << QString();

    QTest::newRow("Passing two dashes, the last is interpreted as a file name")
        << 2
        << QByteArray()
        << (QStringList() << QLatin1String("-")
                          << QLatin1String("-"))
        << QString()
        << QString();

    QTest::newRow("Pass three dashes, the two last gets interpreted as two query arguments")
        << 2
        << QByteArray()
        << (QStringList() << QLatin1String("-")
                          << QLatin1String("-")
                          << QLatin1String("-"))
        << QString()
        << QString();

    QTest::newRow("Load query via data: scheme")
        << 0
        << QByteArray("<e/>\n")
        << (QStringList() << QLatin1String("-is-uri") << QLatin1String("data:application/xml,%3Ce%2F%3E"))
        << QString()
        << QString();

    QTest::newRow("Load query via FTP")
        << 0
        << QByteArray("This was received via FTP\n")
        << (QStringList() << QLatin1String("-is-uri") << QString("ftp://" + QtNetworkSettings::serverName() + "/pub/qxmlquery/viaFtp.xq"))
        << QString()
        << QString();

    QTest::newRow("Load query via HTTP")
        << 0
        << QByteArray("This was received via HTTP.\n")
        << (QStringList() << QLatin1String("-is-uri") << QString("http://" + QtNetworkSettings::serverName() + "/qxmlquery/viaHttp.xq"))
        << QString()
        << QString();

    QTest::newRow("We don't support -format any longer")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-format"))
        << QString()
        << QString();

    QTest::newRow("Run a query which evaluates to the empty sequence.")
        << 0
        << QByteArray("\n")
        << (QStringList() << path + QLatin1String("emptySequence.xq"))
        << QString()
        << QString();

    QTest::newRow("Run a query which evaluates to a single document node with no children.")
        << 0
        << QByteArray("\n")
        << (QStringList() << path + QLatin1String("onlyDocumentNode.xq"))
        << QString()
        << QString();

    QTest::newRow("Invoke with invalid -param value.")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("externalVariable.xq")
                          << QLatin1String("-param")
                          << QLatin1String("EqualSignIsMissing"))
        << QString()
        << QString();

    QTest::newRow("Invoke with colon in variable name.")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("externalVariable.xq")
                          << QLatin1String("-param")
                          << QLatin1String("xs:name=value"))
        << QString()
        << QString();

    QTest::newRow("Invoke with missing name in -param arg.")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("externalVariable.xq")
                          << QLatin1String("-param")
                          << QLatin1String("=value"))
        << QString()
        << QString();

    QTest::newRow("Invoke with -param that has two adjacent equal signs.")
        << 0
        << QByteArray("START =text END\n")
        << (QStringList() << path + QLatin1String("externalStringVariable.xq")
                          << QLatin1String("-param")
                          << QLatin1String("externalString==text"))
        << QString()
        << QString();

    QTest::newRow("Pass in an external variable, but the query doesn't use it.")
        << 0
        << QByteArray("2\n")
        << (QStringList() << path + QLatin1String("onePlusOne.xq")
                          << QLatin1String("-param")
                          << QLatin1String("externalString==text"))
        << QString()
        << QString();

    /* This is how an empty string would have been passed in. */
    QTest::newRow("Invoke with -param that has no value.")
        << 0
        << QByteArray("START  END\n")
        << (QStringList() << path + QLatin1String("externalStringVariable.xq")
                          << QLatin1String("-param")
                          << QLatin1String("externalString="))
        << QString()
        << QString();

    QTest::newRow("Ensure -is-uri can appear after the query filename")
        << 0
        << QByteArray("<e/>\n")
        << (QStringList() << QLatin1String("data:application/xml,%3Ce%2F%3E") << QLatin1String("-is-uri"))
        << QString()
        << QString();

    QTest::newRow("Use a native path")
        << 0
        << QByteArray("2\n")
        << (QStringList() << QDir::toNativeSeparators(path + QLatin1String("onePlusOne.xq")))
        << QString()
        << QString();

    QTest::newRow("Pass in invalid URI")
        << 2
        << QByteArray()
        << (QStringList() << QLatin1String("-is-uri") << QLatin1String("data:application/xml;base64,PGUvg==="))
        << QString()
        << QString();

    /* Not relevant anymore.
    QTest::newRow("A valid, existing query, followed by a bogus one")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("onePlusOne.xq")
                          << path + QLatin1String("doesNotExist.xq"))
        << QString()
        << QString();
        */

    /* Not relevant anymore.
    QTest::newRow("Specify two different query names")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("query1.xq")
                          << path + QLatin1String("query2.xq"))
        << QString()
        << QString();
        */

    // TODO use focus with xquery
    // TODO fail to load focus with xquery
    // TODO focus with URI with xquery
    // TODO focus via FTP or so with xquery


    QTest::newRow("Use -param twice")
        << 0
        << QByteArray("param1 param2\n")
        << (QStringList() << path + QLatin1String("twoVariables.xq")
                          << QLatin1String("-param")
                          << QLatin1String("var1=param1")
                          << QLatin1String("-param")
                          << QLatin1String("var2=param2"))
        << QString()
        << QString();

    QTest::newRow("Use -param thrice")
        << 0
        << QByteArray("param1 param2 third\n")
        << (QStringList() << path + QLatin1String("threeVariables.xq")
                          << QLatin1String("-param")
                          << QLatin1String("var1=param1")
                          << QLatin1String("-param")
                          << QLatin1String("var2=param2")
                          << QLatin1String("-param")
                          << QLatin1String("var3=third"))
        << QString()
        << QString();

    QTest::newRow("Specify the same parameter twice, different values")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("onePlusOne.xq")
                          << QLatin1String("-param")
                          << QLatin1String("duplicated=param1")
                          << QLatin1String("-param")
                          << QLatin1String("duplicated=param2"))
        << QString()
        << QString();

    QTest::newRow("Specify the same parameter twice, same values")
        << 1
        << QByteArray()
        << (QStringList() << path + QLatin1String("onePlusOne.xq")
                          << QLatin1String("-param")
                          << QLatin1String("duplicated=param1")
                          << QLatin1String("-param")
                          << QLatin1String("duplicated=param2"))
        << QString()
        << QString();

    QTest::newRow("Open a non-existing collection.")
        << 2
        << QByteArray()
        << (QStringList() << path + QLatin1String("nonexistingCollection.xq"))
        << QString()
        << QString();

    QTest::newRow("QTBUG-35897: literal sequence")
            << 0
            << QByteArray("someString a b\n")
            << QStringList((path + QStringLiteral("literalsequence.xq")))
            << QString()
            << QString();

    // TODO https?
    // TODO pass external variables that allows space around the equal sign.
    // TODO run fn:trace()
    // TODO Trigger warning
    // TODO what can we do with queries/nodeSequence.xq?
    // TODO trigger serialization error
    // TODO "xmlpatterns e.xq x" gives "binding must equal .."
    //
    // TODO use stdout where it's connected to a non-writable file.
    // TODO specify -format twice, or whatever it's called.
    // TODO query name that starts with "-".
    //
    // TODO Consider what we should do with paths on windows. Stuff like path\filename.xml fails.
    // TODO use invalid URI in query name, xmlpatterns -is-uri 'as1/#(¤/¤)("#'

    // TODO add xmlpatterns file1 file2 file3
    // TODO add xmlpatterns -is-uri file1 file2 file3
}

void tst_XmlPatterns::createNonWritable(const QString &name)
{
    /* Create an existing, empty, non-writable file. */
    QFile outFile(name);
    QVERIFY(outFile.open(QIODevice::ReadWrite));
    outFile.write(QByteArray("1"));
    QVERIFY(outFile.resize(0));
    outFile.close();
    QVERIFY(outFile.setPermissions(QFile::Permissions(QFile::ReadOwner)));
}

void tst_XmlPatterns::removeNonWritable(QFile &outFile)
{
    /* Kill off temporary files. */
    if(!outFile.remove())
    {
        /* Since one file is used for testing that we can handle non-writable file by
         * changing the permissions, we need to revert it such that we can remove it. */
        outFile.setPermissions(QFile::WriteOwner);
        outFile.remove();
    }
}

/*!
 Check that we gracefully handle writing out to stdout
 when the latter is not writable.
 */
void tst_XmlPatterns::stdoutFailure() const
{
#ifdef QT_NO_PROCESS
    QSKIP("Skipping test due to not having process support");
#else
    return; // TODO It's really hard to write testing code for this.

    const QString outName(QLatin1String("stdoutFailure.out"));
    createNonWritable(outName);

    QProcess process;
    // If we enable this line, waitForFinished() fails.
    //process.setStandardOutputFile(outName);

    process.setWorkingDirectory(QDir::current().absoluteFilePath(QString()));
    process.start(m_command, QStringList(QFINDTESTDATA("queries/onePlusOne.xq")));

    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY(process.waitForFinished());

    QFile outFile(outName);
    QVERIFY(outFile.open(QIODevice::ReadOnly));
    QCOMPARE(outFile.readAll(), QByteArray());

    QCOMPARE(process.exitCode(), 1);

    removeNonWritable(outFile);
#endif // QT_NO_PROCESS
}

void tst_XmlPatterns::cleanupTestCase() const
{
    /* Remove temporaries that we create. */
    QStringList files;
    files << QLatin1String("existingContent.out")
          << QLatin1String("notWritable.out")
          << QLatin1String("output1");

    for(int i = 0; i < files.count(); ++i)
    {
        QFile file(files.at(i));
        removeNonWritable(file);
    }

    QCOMPARE(m_generatedTests, 0);
}

void tst_XmlPatterns::xsltSupport()
{
    xquerySupport();
}

void tst_XmlPatterns::xsltSupport_data() const
{
    if(m_dontRun)
        QSKIP("The command line utility is not in the path.");

    QString spath = QFINDTESTDATA("stylesheets/");
    QString qpath = QFINDTESTDATA("queries/");

    QTest::addColumn<int>("expectedExitCode");
    QTest::addColumn<QByteArray>("expectedQueryOutput");
    QTest::addColumn<QStringList>("arguments");
    QTest::addColumn<QString>("cwd");
    QTest::addColumn<QString>("outputFile");

    QTest::newRow("Evaluate a stylesheet, with no context document")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("stylesheets/onlyRootTemplate.xsl"))
        << QString()
        << QString();

    QTest::newRow("Pass in a stylesheet file which contains an XQuery query")
        << 2
        << QByteArray()
        << (QStringList() << spath + QLatin1String("queryAsStylesheet.xsl")
                          << qpath + QLatin1String("simpleDocument.xml"))
        << QString()
        << QString();

    QTest::newRow("Pass in a stylesheet file and a focus file which doesn't exist")
        << 2
        << QByteArray()
        << (QStringList() << QLatin1String("stylesheets/onlyRootTemplate.xsl")
                          << QLatin1String("doesNotExist.Nope.xml"))
        << QString()
        << QString();

    QTest::newRow("-initial-template doesn't work with XQueries.")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-initial-template")
                          << QLatin1String("name")
                          << qpath + QLatin1String("onePlusOne.xq"))
        << QString()
        << QString();

    QTest::newRow("-initial-template must be followed by a value")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-initial-template")
                          << QLatin1String("stylesheets/onlyRootTemplate.xsl"))
        << QString()
        << QString();

    QTest::newRow("-initial-template must be followed by a value(#2)")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("stylesheets/onlyRootTemplate.xsl")
                          << QLatin1String("-initial-template"))
        << QString()
        << QString();

    QTest::newRow("Invalid template name")
        << 1
        << QByteArray()
        << (QStringList() << QLatin1String("-initial-template")
                          << QLatin1String("abc:def")
                          << QLatin1String("stylesheets/onlyRootTemplate.xsl"))
        << QString()
        << QString();

    QTest::newRow("Specify a named template, that exists")
        << 0
        << QByteArray("named-template")
        << (QStringList() << QLatin1String("-no-format")
                          << QLatin1String("-initial-template")
                          << QLatin1String("main")
                          << spath + QLatin1String("namedAndRootTemplate.xsl")
                          << spath + QLatin1String("documentElement.xml"))
        << QString()
        << QString();

    QTest::newRow("Specify a named template, that does not exists")
        << 0
        << QByteArray("root-template")
        << (QStringList() << QLatin1String("-no-format")
                          << QLatin1String("-initial-template")
                          << QLatin1String("no-template-by-this-name")
                          << spath + QLatin1String("namedAndRootTemplate.xsl")
                          << spath + QLatin1String("documentElement.xml"))
        << QString()
        << QString();

    QTest::newRow("Call a named template, and use no focus.")
        << 0
        << QByteArray("named-template")
        << (QStringList() << QLatin1String("-no-format")
                          << QLatin1String("-initial-template")
                          << QLatin1String("main")
                          << spath + QLatin1String("namedAndRootTemplate.xsl"))
        << QString()
        << QString();

    QTest::newRow("Call a named template, and use no focus.")
        << 0
        << QByteArray("namespaced-template")
        << (QStringList() << QLatin1String("-no-format")
                          << QLatin1String("-initial-template")
                          << QLatin1String("{http://example.com/NS}main")
                          << spath + QLatin1String("namedAndRootTemplate.xsl"))
        << QString()
        << QString();

    QTest::newRow("Invoke a template, and use/pass parameters.")
        << 0
        << QByteArray("defParam overridedDefaultedParam implicitlyRequiredValue\n")
        << (QStringList() << QLatin1String("-initial-template")
                          << QLatin1String("main")
                          << spath + QLatin1String("useParameters.xsl")
                          << QLatin1String("-param")
                          << QLatin1String("overridedDefaultedParam=overridedDefaultedParam")
                          << QLatin1String("-param")
                          << QLatin1String("implicitlyRequiredValue=implicitlyRequiredValue"))
        << QString()
        << QString();

    QTest::newRow("Use a simplified stylesheet module")
        << 0
        << QByteArray("<output>some text</output>\n")
        << (QStringList() << spath + QLatin1String("simplifiedStylesheetModule.xsl")
                          << spath + QLatin1String("simplifiedStylesheetModule.xml"))
        << QString()
        << QString();

    QTest::newRow("Not well-formed stylesheet, causes crash in coloring code.")
        << 2
        << QByteArray()
        << (QStringList() << spath + QLatin1String("notWellformed.xsl")
                          << qpath + QLatin1String("simpleDocument.xml"))
        << QString()
        << QString();

    QTest::newRow("Not well-formed instance document, causes crash in coloring code.")
        << 2
        << QByteArray()
        << (QStringList() << spath + QLatin1String("bool070.xsl")
                          << spath + QLatin1String("bool070.xml"))
        << QString()
        << QString();

    // TODO test -is-uris with context
    // TODO fail to load focus document when using XSL-T
    // TODO fail to load focus document when using XQuery
    // TODO focus via FTP or so with xquery
    // TODO use URI in focus
    // TODO use invalid URI in focus

    // TODO invoke a template which has required params.
}

/*
    Return a copy of some stderr text with some irrelevant things filtered.
*/
QString tst_XmlPatterns::filterStderr(const QString &in)
{
    static const QList<QRegExp> irrelevant = QList<QRegExp>()

        // specific filenames
        << QRegExp(QLatin1String("file:\\/\\/.*(\\.xq|\\.gccxml|\\.xml|\\.xsl|-)(,|:)"))

        // warning messages about old-style plugins
        << QRegExp(QLatin1String("Old plugin format found in lib [^\n]+\n"))
        << QRegExp(QLatin1String("Qt plugin loader: Compatibility plugin [^\n]+\n"))
        << QRegExp(QLatin1String("Unimplemented code.\n"))
    ;

    QString out = in;
    for (const QRegExp& rx : irrelevant)
        out = out.remove(rx);

    return out;
}

QTEST_MAIN(tst_XmlPatterns)

#include "tst_xmlpatterns.moc"

// vim: et:ts=4:sw=4:sts=4
