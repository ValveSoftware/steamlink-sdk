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

#include <QDir>
#include <QEventLoop>
#include <QPair>
#include <QtDebug>

#include "ExitCode.h"

#include "Worker.h"

using namespace QPatternistSDK;

const char *const Worker::m_indent = "        ";

Worker::Worker(QEventLoop &ev,
               const QFileInfo &baseline,
               const QFileInfo &result) : m_finishedCount(0)
                                        , m_baselineFile(baseline)
                                        , m_resultFile(result)
                                        , m_eventLoop(ev)
{
}

void Worker::list(QTextStream &out, const QString &msg, QStringList &list)
{
    Q_ASSERT(!msg.isEmpty());

    if(list.isEmpty())
        return;

    list.sort(); /* Make it pretty, and easy to read. */

    out << msg << ":\n";

    const QStringList::const_iterator end(list.constEnd());
    QStringList::const_iterator it(list.constBegin());

    for(; it != end; ++it)
        out << m_indent << qPrintable(*it) << '\n';
}

static inline int count(const ResultThreader::Hash &list, const TestResult::Status stat)
{
    const ResultThreader::Hash::const_iterator end(list.constEnd());
    ResultThreader::Hash::const_iterator it(list.constBegin());
    int result = 0;

    for(; it != end; ++it)
    {
        if(it.value() == stat)
            ++result;
    }

    return result;
}

void Worker::threadFinished()
{
    ++m_finishedCount;
    Q_ASSERT(m_finishedCount == 1 || m_finishedCount == 2);

    const ResultThreader *const handler = static_cast<ResultThreader *>(sender());
    Q_ASSERT(handler);

    switch(handler->type())
    {
        case ResultThreader::Baseline:
        {
            m_baseline = handler->result();
            break;
        }
        case ResultThreader::Result:
            m_result = handler->result();
    }

    if(m_finishedCount == 1) /* One thread's missing. */
        return;

    /* Ok, both threads have now finished, and we got their results in m_result and m_baseline. */

    /* No matter how this function exits, we want to delete this Worker. */
    deleteLater();

    ResultThreader::Hash::const_iterator itA(m_result.constBegin());
    ResultThreader::Hash::const_iterator itB(m_baseline.constBegin());
    const ResultThreader::Hash::const_iterator endA(m_result.constEnd());
    const ResultThreader::Hash::const_iterator endB(m_baseline.constEnd());
    const int baselineCount = m_baseline.count();
    const int resultCount = m_result.count();

    /* If you want useful output, change the QTextStream to use stderr. */
    //QTextStream err(stderr);
    QByteArray out;
    QTextStream err(&out);

    if(resultCount < baselineCount)
    {
        err << qPrintable(QString(QLatin1String("WARNING: Test result contains %1 reports, "
                                                "but the baseline contains %2, a DECREASE "
                                                "of %3 tests.\n"))
                                  .arg(resultCount)
                                  .arg(baselineCount)
                                  .arg(resultCount - baselineCount));
    }
    else if(resultCount > baselineCount)
    {
        err << qPrintable(QString(QLatin1String("NOTE: The number of tests run is more than what "
                                                "the baseline specifies. Run was %1 test cases, the "
                                                "baseline specifies %2; an increase of %3 tests.\n"))
                                  .arg(resultCount)
                                  .arg(baselineCount)
                                  .arg(resultCount - baselineCount));
    }

    for(; itA != endA; ++itA)
    {
        const TestResult::Status result = itA.value();
        const TestResult::Status baseline = m_baseline.value(itA.key());

        if(result == baseline) /* We have no change. */
        {
            if(result == TestResult::NotTested)
                m_notTested.append(itA.key());
            else
                continue;
        }
        else if(baseline == TestResult::Pass && result == TestResult::Fail)
            m_unexpectedFailures.append(itA.key());
        else if(baseline == TestResult::Fail && result == TestResult::Pass)
            m_unexpectedPasses.append(itA.key());
    }

    list(err, QLatin1String("Not tested"),           m_notTested);
    list(err, QLatin1String("Unexpected failures"),  m_unexpectedFailures);
    list(err, QLatin1String("Unexpected passes"),    m_unexpectedPasses);

    err << "SUMMARY:\n";
    typedef QPair<QString, int> Info;
    typedef QList<Info> InfoList;
    InfoList info;

    const int totFail       = count(m_result, TestResult::Fail);
    const int totPass       = count(m_result, TestResult::Pass);
    const int total         = resultCount;
    const int notTested     = m_notTested.count();
    const int percentage    = int((static_cast<double>(totPass) / total) * 100);

    Q_ASSERT_X(percentage >= 0 && percentage <= 100, Q_FUNC_INFO,
               qPrintable(QString(QLatin1String("Percentage was: %1")).arg(percentage)));

    info.append(Info(QLatin1String("Total"),                total));
    info.append(Info(QLatin1String("Failures"),             totFail));
    info.append(Info(QLatin1String("Passes"),               totPass));
    info.append(Info(QLatin1String("Not tested"),           notTested));
    info.append(Info(QLatin1String("Pass percentage(%)"),   percentage));
    info.append(Info(QLatin1String("Unexpected failures"),  m_unexpectedFailures.count()));
    info.append(Info(QLatin1String("Unexpected passes"),    m_unexpectedPasses.count()));

    const InfoList::const_iterator end(info.constEnd());
    InfoList::const_iterator it(info.constBegin());

    /* List the statistics nicely in a row with padded columns. */
    for(; it != end; ++it)
    {
        const QString result((((*it).first) + QLatin1Char(':')).leftJustified(22, QLatin1Char(' ')));
        err << m_indent << qPrintable(result) << (*it).second << '\n';
    }

    if(!m_unexpectedFailures.isEmpty())
    {
        err << "FAILURE: Regressions discovered, baseline was not updated.\n";
        err.flush();
        QTextStream(stderr) << out;
        m_eventLoop.exit(ExitCode::Regression);
        return;
    }
    else if(m_unexpectedPasses.isEmpty() && baselineCount == resultCount)
    {
        err << "Result was identical to the baseline, baseline was not updated.\n";
        err.flush();
        QTextStream(stderr) << out;
        m_eventLoop.exit(ExitCode::Success);
        return;
    }

    /* Ok, we got unexpected successes and no regressions: let's update the baseline. */

    QFile resultFile(m_resultFile.absoluteFilePath());

    /* Remove the old file, otherwise QFile::copy() will fail. */
    QDir baselineDir(m_baselineFile.absolutePath());
    baselineDir.remove(m_baselineFile.fileName());

    if(resultFile.copy(m_baselineFile.absoluteFilePath()))
    {
        /* Give a detailed message of what's going on. */
        if(resultCount > baselineCount)
            err << "More tests was run than specified in the baseline, updating the baseline.\n";
        else
            err << "Improvement, the baseline was updated.\n";

        /* We actually flag this as an error, because the new baseline must be submitted. */
        err.flush();
        QTextStream(stderr) << out;
        m_eventLoop.exit(ExitCode::Regression);
        return;
    }
    else
    {
        err << qPrintable(QString(QLatin1String("Encountered error when updating "
                                                "the baseline: %1\n"))
                                  .arg(resultFile.errorString()));
        err.flush();
        QTextStream(stderr) << out;
        m_eventLoop.exit(ExitCode::WriteError);
        return;
    }
}

// vim: et:ts=4:sw=4:sts=4
