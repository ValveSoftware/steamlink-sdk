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

#include <QProcess>
#include <cstdlib>

#include "TestSuite.h"
#include "TestSuiteResult.h"
#include "XMLWriter.h"
#include "ExitCode.h"
#include "Worker.h"
#include "private/qautoptr_p.h"
#include "tst_suitetest.h"

using namespace QPatternistSDK;

tst_SuiteTest::tst_SuiteTest(const SuiteType suiteType,
                             const bool alwaysRun) : m_existingBaseline(QFINDTESTDATA("Baseline.xml"))
                                                   , m_candidateBaseline(QDir::current().filePath("CandidateBaseline.xml"))
                                                   , m_abortRun(!alwaysRun && !QFile::exists(QLatin1String("runTests")))
                                                   , m_suiteType(suiteType)
{
}

/*!
 Returns an absolute path to the XQTS catalog, or flags a failure using
 QTestLib's mechanisms.
 */
void tst_SuiteTest::initTestCase()
{
    catalogPath(m_catalogPath);
}

/*!
  Just runs the test suite and writes the result to m_candidateBaseline.
 */
void tst_SuiteTest::runTestSuite() const
{
    if(m_abortRun)
        QSKIP("The test suite is not available, no tests are run.");

    QByteArray range = qgetenv("XMLPATTERNSXQTS_TESTRANGE");
    char *endptr;
    TreeItem::executeRange.first = strtol(range.constData(), &endptr, 10);
    long e = 0;
    if (endptr - range.constData() < range.size())
        e = strtol(endptr + 1, &endptr, 10);
    TreeItem::executeRange.second = (e == 0 ? INT_MAX : e);
    QString errMsg;
    const QFileInfo fi(m_catalogPath);
    const QUrl catalogPath(QUrl::fromLocalFile(fi.absoluteFilePath()));

    TestSuite::SuiteType suiteType(TestSuite::XQuerySuite);
    switch (m_suiteType) {
    case XQuerySuite:
        suiteType = TestSuite::XQuerySuite;
        break;
    case XsltSuite:
        suiteType = TestSuite::XsltSuite;
        break;
    case XsdSuite:
        suiteType = TestSuite::XsdSuite;
        break;
    default:
        break;
    }

    TestSuite *const ts = TestSuite::openCatalog(catalogPath, errMsg, true, suiteType);

    QVERIFY2(ts, qPrintable(QString::fromLatin1("Failed to open the catalog, maybe it doesn't exist or is broken: %1").arg(errMsg)));

    /* Run the tests, and serialize the result(as according to XQTSResult.xsd) to standard out. */
    TestSuiteResult *const result = ts->runSuite();
    QVERIFY(result);

    QFile out(m_candidateBaseline);
    QVERIFY(out.open(QIODevice::WriteOnly));

    XMLWriter serializer(&out);
    result->toXML(serializer);

    delete result;
    delete ts;
}

void tst_SuiteTest::checkTestSuiteResult() const
{
    if(m_abortRun)
        QSKIP("This test takes too long time to run on the majority of platforms.");

    typedef QList<QFileInfo> QFileInfoList;

    const QFileInfo baseline(m_existingBaseline);
    const QFileInfo result(m_candidateBaseline);
    QFileInfoList list;
    list.append(baseline);
    list.append(result);

    const QFileInfoList::const_iterator end(list.constEnd());

    QEventLoop eventLoop;
    const QPatternist::AutoPtr<Worker> worker(new Worker(eventLoop, m_existingBaseline, result));

    /* Passed to ResultThreader so it knows what kind of file it is handling. */
    ResultThreader::Type type = ResultThreader::Baseline;

    for(QFileInfoList::const_iterator it(list.constBegin()); it != end; ++it)
    {
        QFileInfo i(*it);
        i.makeAbsolute();

        QVERIFY2(i.exists(), qPrintable(QString::fromLatin1("File %1 does not exist.")
                                                            .arg(i.fileName())));

        QFile *const file = new QFile(i.absoluteFilePath(), worker.data());

        QVERIFY2(file->open(QIODevice::ReadOnly), qPrintable(QString::fromLatin1("Could not open file %1 for reading.")
                                                             .arg(i.fileName())));

        ResultThreader *handler = new ResultThreader(eventLoop, file, type, worker.data());

        QObject::connect(handler, SIGNAL(finished()), worker.data(), SLOT(threadFinished()));

        handler->start(); /* Start the thread. It now parses the file
                             and emits threadFinished() when done. */
        type = ResultThreader::Result;
    }

    const int exitCode = eventLoop.exec();

    QCOMPARE(exitCode, 0);
}

bool tst_SuiteTest::dontRun() const
{
    return m_abortRun;
}

// vim: et:ts=4:sw=4:sts=4
