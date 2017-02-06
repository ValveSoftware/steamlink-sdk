/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "quicktestresult_p.h"
#include <QtTest/qtestcase.h>
#include <QtTest/qtestsystem.h>
#include <QtTest/private/qtestblacklist_p.h>
#include <QtTest/private/qtestresult_p.h>
#include <QtTest/private/qtesttable_p.h>
#include <QtTest/private/qtestlog_p.h>
#include "qtestoptions_p.h"
#include <QtTest/qbenchmark.h>
// qbenchmark_p.h pulls windows.h via 3rd party; prevent it from defining
// the min/max macros which would clash with qnumeric_p.h's usage of min()/max().
#if defined(Q_OS_WIN32) && !defined(NOMINMAX)
#  define NOMINMAX
#endif
#include <QtTest/private/qbenchmark_p.h>
#include <QtCore/qset.h>
#include <QtCore/qmap.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/QUrl>
#include <QtCore/QDir>
#include <QtQuick/qquickwindow.h>
#include <QtGui/qvector3d.h>
#include <QtQml/private/qqmlglobal_p.h>
#include <private/qv4qobjectwrapper_p.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

static const char *globalProgramName = 0;
static bool loggingStarted = false;
static QBenchmarkGlobalData globalBenchmarkData;

extern bool qWaitForSignal(QObject *obj, const char* signal, int timeout = 5000);

class Q_QUICK_TEST_EXPORT QuickTestImageObject : public QObject
{
    Q_OBJECT
public:
    QuickTestImageObject(const QImage& img, QObject *parent = 0)
        : QObject(parent)
        , m_image(img)
    {
    }

    ~QuickTestImageObject() {}

public Q_SLOTS:
    int red(int x, int y) const
    {
        return pixel(x, y).value<QColor>().red();
    }

    int green(int x, int y) const
    {
        return pixel(x, y).value<QColor>().green();
    }

    int blue(int x, int y) const
    {
        return pixel(x, y).value<QColor>().blue();
    }

    int alpha(int x, int y) const
    {
        return pixel(x, y).value<QColor>().alpha();
    }

    QVariant pixel(int x, int y) const
    {
        if (m_image.isNull()
         || x >= m_image.width()
         || y >= m_image.height()
         || x < 0
         || y < 0
         || x * y >= m_image.width() * m_image.height())
            return QVariant();

        return QColor::fromRgba(m_image.pixel(QPoint(x, y)));
    }

    bool equals(QuickTestImageObject *other) const
    {
        if (!other)
            return m_image.isNull();

        return m_image == other->m_image;
    }
private:
    QImage m_image;
};

class QuickTestResultPrivate
{
public:
    QuickTestResultPrivate()
        : table(0)
        , benchmarkIter(0)
        , benchmarkData(0)
        , iterCount(0)
    {
    }
    ~QuickTestResultPrivate()
    {
        delete table;
        delete benchmarkIter;
        delete benchmarkData;
    }

    QByteArray intern(const QString &str);

    QString testCaseName;
    QString functionName;
    QSet<QByteArray> internedStrings;
    QTestTable *table;
    QTest::QBenchmarkIterationController *benchmarkIter;
    QBenchmarkTestMethodData *benchmarkData;
    int iterCount;
    QList<QBenchmarkResult> results;
};

QByteArray QuickTestResultPrivate::intern(const QString &str)
{
    QByteArray bstr = str.toUtf8();
    return *(internedStrings.insert(bstr));
}

QuickTestResult::QuickTestResult(QObject *parent)
    : QObject(parent), d_ptr(new QuickTestResultPrivate)
{
    if (!QBenchmarkGlobalData::current)
        QBenchmarkGlobalData::current = &globalBenchmarkData;
}

QuickTestResult::~QuickTestResult()
{
}

/*!
    \qmlproperty string TestResult::testCaseName

    This property defines the name of current TestCase element
    that is running test cases.

    \sa functionName
*/
QString QuickTestResult::testCaseName() const
{
    Q_D(const QuickTestResult);
    return d->testCaseName;
}

void QuickTestResult::setTestCaseName(const QString &name)
{
    Q_D(QuickTestResult);
    d->testCaseName = name;
    emit testCaseNameChanged();
}

/*!
    \qmlproperty string TestResult::functionName

    This property defines the name of current test function
    within a TestCase element that is running.  If this string is
    empty, then no function is currently running.

    \sa testCaseName
*/
QString QuickTestResult::functionName() const
{
    Q_D(const QuickTestResult);
    return d->functionName;
}

void QuickTestResult::setFunctionName(const QString &name)
{
    Q_D(QuickTestResult);
    if (!name.isEmpty()) {
        if (d->testCaseName.isEmpty()) {
            QTestResult::setCurrentTestFunction
                (d->intern(name).constData());
        } else {
            QString fullName = d->testCaseName + QLatin1String("::") + name;
            QTestResult::setCurrentTestFunction
                (d->intern(fullName).constData());
            QTestPrivate::checkBlackLists(fullName.toUtf8().constData(), 0);
        }
    } else {
        QTestResult::setCurrentTestFunction(0);
    }
    d->functionName = name;
    emit functionNameChanged();
}

/*!
    \qmlproperty string TestResult::dataTag

    This property defines the tag for the current row in a
    data-driven test, or an empty string if not a data-driven test.
*/
QString QuickTestResult::dataTag() const
{
    const char *tag = QTestResult::currentDataTag();
    if (tag)
        return QString::fromUtf8(tag);
    else
        return QString();
}

void QuickTestResult::setDataTag(const QString &tag)
{
    if (!tag.isEmpty()) {
        QTestData *data = &(QTest::newRow(tag.toUtf8().constData()));
        QTestResult::setCurrentTestData(data);
        QTestPrivate::checkBlackLists((testCaseName() + QLatin1String("::") + functionName()).toUtf8().constData(), tag.toUtf8().constData());
        emit dataTagChanged();
    } else {
        QTestResult::setCurrentTestData(0);
    }
}

/*!
    \qmlproperty bool TestResult::failed

    This property returns true if the current test function (or
    current test data row for a data-driven test) has failed;
    false otherwise.  The fail state is reset when functionName
    is changed or finishTestDataCleanup() is called.

    \sa skipped
*/
bool QuickTestResult::isFailed() const
{
    return QTestResult::currentTestFailed();
}

/*!
    \qmlproperty bool TestResult::skipped

    This property returns true if the current test function was
    marked as skipped; false otherwise.

    \sa failed
*/
bool QuickTestResult::isSkipped() const
{
    return QTestResult::skipCurrentTest();
}

void QuickTestResult::setSkipped(bool skip)
{
    QTestResult::setSkipCurrentTest(skip);
    if (!skip)
        QTestResult::setBlacklistCurrentTest(false);
    emit skippedChanged();
}

/*!
    \qmlproperty int TestResult::passCount

    This property returns the number of tests that have passed.

    \sa failCount, skipCount
*/
int QuickTestResult::passCount() const
{
    return QTestLog::passCount();
}

/*!
    \qmlproperty int TestResult::failCount

    This property returns the number of tests that have failed.

    \sa passCount, skipCount
*/
int QuickTestResult::failCount() const
{
    return QTestLog::failCount();
}

/*!
    \qmlproperty int TestResult::skipCount

    This property returns the number of tests that have been skipped.

    \sa passCount, failCount
*/
int QuickTestResult::skipCount() const
{
    return QTestLog::skipCount();
}

/*!
    \qmlproperty list<string> TestResult::functionsToRun

    This property returns the list of function names to be run.
*/
QStringList QuickTestResult::functionsToRun() const
{
    return QTest::testFunctions;
}

/*!
    \qmlmethod TestResult::reset()

    Resets all pass/fail/skip counters and prepare for testing.
*/
void QuickTestResult::reset()
{
    if (!globalProgramName)     // Only if run via qmlviewer.
        QTestResult::reset();
}

/*!
    \qmlmethod TestResult::startLogging()

    Starts logging to the test output stream and writes the
    test header.

    \sa stopLogging()
*/
void QuickTestResult::startLogging()
{
    // The program name is used for logging headers and footers if it
    // is set.  Otherwise the test case name is used.
    if (loggingStarted)
        return;
    QTestLog::startLogging();
    loggingStarted = true;
}

/*!
    \qmlmethod TestResult::stopLogging()

    Writes the test footer to the test output stream and then stops logging.

    \sa startLogging()
*/
void QuickTestResult::stopLogging()
{
    Q_D(QuickTestResult);
    if (globalProgramName)
        return;     // Logging will be stopped by setProgramName(0).
    QTestResult::setCurrentTestObject(d->intern(d->testCaseName).constData());
    QTestLog::stopLogging();
}

void QuickTestResult::initTestTable()
{
    Q_D(QuickTestResult);
    delete d->table;
    d->table = new QTestTable;
    //qmltest does not really need the column for data driven test
    //add this to avoid warnings.
    d->table->addColumn(qMetaTypeId<QString>(), "qmltest_dummy_data_column");
}

void QuickTestResult::clearTestTable()
{
    Q_D(QuickTestResult);
    delete d->table;
    d->table = 0;
}

void QuickTestResult::finishTestData()
{
    QTestResult::finishedCurrentTestData();
}

void QuickTestResult::finishTestDataCleanup()
{
    QTestResult::finishedCurrentTestDataCleanup();
}

void QuickTestResult::finishTestFunction()
{
    QTestResult::finishedCurrentTestFunction();
}

static QString qtestFixUrl(const QUrl &location)
{
    if (location.isLocalFile()) // Use QUrl's logic for Windows drive letters.
        return QDir::toNativeSeparators(location.toLocalFile());
    return location.toString();
}

void QuickTestResult::fail
    (const QString &message, const QUrl &location, int line)
{
    QTestResult::addFailure(message.toLatin1().constData(),
                            qtestFixUrl(location).toLatin1().constData(), line);
}

bool QuickTestResult::verify
    (bool success, const QString &message, const QUrl &location, int line)
{
    if (!success && message.isEmpty()) {
        return QTestResult::verify
            (success, "verify()", "",
             qtestFixUrl(location).toLatin1().constData(), line);
    } else {
        return QTestResult::verify
            (success, message.toLatin1().constData(), "",
             qtestFixUrl(location).toLatin1().constData(), line);
    }
}

bool QuickTestResult::fuzzyCompare(const QVariant &actual, const QVariant &expected, qreal delta)
{
    if (actual.type() == QVariant::Color || expected.type() == QVariant::Color) {
        if (!actual.canConvert(QVariant::Color) || !expected.canConvert(QVariant::Color))
            return false;

        //fuzzy color comparison
        QColor act;
        QColor exp;
        bool ok(false);

        QVariant var = QQml_colorProvider()->colorFromString(actual.toString(), &ok);
        if (!ok)
            return false;
        act = var.value<QColor>();

        QQml_colorProvider()->colorFromString(expected.toString(), &ok);
        if (!ok)
            return false;
        exp = var.value<QColor>();

        return ( qAbs(act.red() - exp.red()) <= delta
              && qAbs(act.green() - exp.green()) <= delta
              && qAbs(act.blue() - exp.blue()) <= delta
              && qAbs(act.alpha() - exp.alpha()) <= delta);
    } else {
        //number comparison
        bool ok = true;
        qreal act = actual.toFloat(&ok);
        if (!ok)
            return false;

        qreal exp = expected.toFloat(&ok);
        if (!ok)
            return false;

        return (qAbs(act - exp) <= delta);
    }

    return false;
}

void QuickTestResult::stringify(QQmlV4Function *args)
{
    if (args->length() < 1)
        args->setReturnValue(QV4::Encode::null());

    QV4::Scope scope(args->v4engine());
    QV4::ScopedValue value(scope, (*args)[0]);

    QString result;

    //Check for Object Type
    if (value->isObject()
        && !value->as<QV4::FunctionObject>()
        && !value->as<QV4::ArrayObject>()) {
        QVariant v = scope.engine->toVariant(value, QMetaType::UnknownType);
        if (v.isValid()) {
            switch (v.type()) {
            case QVariant::Vector3D:
            {
                QVector3D v3d = v.value<QVector3D>();
                result = QString::fromLatin1("Qt.vector3d(%1, %2, %3)").arg(v3d.x()).arg(v3d.y()).arg(v3d.z());
                break;
            }
            default:
                result = v.toString();
            }

        } else {
            result = QLatin1String("Object");
        }
    }

    if (result.isEmpty()) {
        QString tmp = value->toQStringNoThrow();
        if (value->as<QV4::ArrayObject>())
            result.append(QString::fromLatin1("[%1]").arg(tmp));
        else
            result.append(tmp);
    }

    args->setReturnValue(QV4::Encode(args->v4engine()->newString(result)));
}

bool QuickTestResult::compare
    (bool success, const QString &message,
     const QVariant &val1, const QVariant &val2,
     const QUrl &location, int line)
{
    return QTestResult::compare
        (success, message.toLocal8Bit().constData(),
         QTest::toString(val1.toString().toLatin1().constData()),
         QTest::toString(val2.toString().toLatin1().constData()),
         "", "",
         qtestFixUrl(location).toLatin1().constData(), line);
}

void QuickTestResult::skip
    (const QString &message, const QUrl &location, int line)
{
    QTestResult::addSkip(message.toLatin1().constData(),
                         qtestFixUrl(location).toLatin1().constData(), line);
    QTestResult::setSkipCurrentTest(true);
}

bool QuickTestResult::expectFail
    (const QString &tag, const QString &comment, const QUrl &location, int line)
{
    return QTestResult::expectFail
        (tag.toLatin1().constData(),
         QTest::toString(comment.toLatin1().constData()),
         QTest::Abort, qtestFixUrl(location).toLatin1().constData(), line);
}

bool QuickTestResult::expectFailContinue
    (const QString &tag, const QString &comment, const QUrl &location, int line)
{
    return QTestResult::expectFail
        (tag.toLatin1().constData(),
         QTest::toString(comment.toLatin1().constData()),
         QTest::Continue, qtestFixUrl(location).toLatin1().constData(), line);
}

void QuickTestResult::warn(const QString &message, const QUrl &location, int line)
{
    QTestLog::warn(message.toLatin1().constData(), qtestFixUrl(location).toLatin1().constData(), line);
}

void QuickTestResult::ignoreWarning(const QString &message)
{
    QTestLog::ignoreMessage(QtWarningMsg, message.toLatin1().constData());
}

void QuickTestResult::wait(int ms)
{
    QTest::qWait(ms);
}

void QuickTestResult::sleep(int ms)
{
    QTest::qSleep(ms);
}

bool QuickTestResult::waitForRendering(QQuickItem *item, int timeout)
{
    Q_ASSERT(item);

    return qWaitForSignal(item->window(), SIGNAL(frameSwapped()), timeout);
}

void QuickTestResult::startMeasurement()
{
    Q_D(QuickTestResult);
    delete d->benchmarkData;
    d->benchmarkData = new QBenchmarkTestMethodData();
    QBenchmarkTestMethodData::current = d->benchmarkData;
    d->iterCount = (QBenchmarkGlobalData::current->measurer->needsWarmupIteration()) ? -1 : 0;
    d->results.clear();
}

void QuickTestResult::beginDataRun()
{
    QBenchmarkTestMethodData::current->beginDataRun();
}

void QuickTestResult::endDataRun()
{
    Q_D(QuickTestResult);
    QBenchmarkTestMethodData::current->endDataRun();
    if (d->iterCount > -1)  // iteration -1 is the warmup iteration.
        d->results.append(QBenchmarkTestMethodData::current->result);

    if (QBenchmarkGlobalData::current->verboseOutput) {
        if (d->iterCount == -1) {
            qDebug() << "warmup stage result      :" << QBenchmarkTestMethodData::current->result.value;
        } else {
            qDebug() << "accumulation stage result:" << QBenchmarkTestMethodData::current->result.value;
        }
    }
}

bool QuickTestResult::measurementAccepted()
{
    return QBenchmarkTestMethodData::current->resultsAccepted();
}

static QBenchmarkResult qMedian(const QList<QBenchmarkResult> &container)
{
    const int count = container.count();
    if (count == 0)
        return QBenchmarkResult();

    if (count == 1)
        return container.at(0);

    QList<QBenchmarkResult> containerCopy = container;
    std::sort(containerCopy.begin(), containerCopy.end());

    const int middle = count / 2;

    // ### handle even-sized containers here by doing an aritmetic mean of the two middle items.
    return containerCopy.at(middle);
}

bool QuickTestResult::needsMoreMeasurements()
{
    Q_D(QuickTestResult);
    ++(d->iterCount);
    if (d->iterCount < QBenchmarkGlobalData::current->adjustMedianIterationCount())
        return true;
    if (QBenchmarkTestMethodData::current->resultsAccepted())
        QTestLog::addBenchmarkResult(qMedian(d->results));
    return false;
}

void QuickTestResult::startBenchmark(RunMode runMode, const QString &tag)
{
    QBenchmarkTestMethodData::current->result = QBenchmarkResult();
    QBenchmarkTestMethodData::current->resultAccepted = false;
    QBenchmarkGlobalData::current->context.tag = tag;
    QBenchmarkGlobalData::current->context.slotName = functionName();

    Q_D(QuickTestResult);
    delete d->benchmarkIter;
    d->benchmarkIter = new QTest::QBenchmarkIterationController
        (QTest::QBenchmarkIterationController::RunMode(runMode));
}

bool QuickTestResult::isBenchmarkDone() const
{
    Q_D(const QuickTestResult);
    if (d->benchmarkIter)
        return d->benchmarkIter->isDone();
    else
        return true;
}

void QuickTestResult::nextBenchmark()
{
    Q_D(QuickTestResult);
    if (d->benchmarkIter)
        d->benchmarkIter->next();
}

void QuickTestResult::stopBenchmark()
{
    Q_D(QuickTestResult);
    delete d->benchmarkIter;
    d->benchmarkIter = 0;
}

QObject *QuickTestResult::grabImage(QQuickItem *item)
{
    if (item && item->window()) {
        QQuickWindow *window = item->window();
        QImage grabbed = window->grabWindow();
        QRectF rf(item->x(), item->y(), item->width(), item->height());
        rf = rf.intersected(QRectF(0, 0, grabbed.width(), grabbed.height()));
        return new QuickTestImageObject(grabbed.copy(rf.toAlignedRect()));
    }
    return 0;
}

QObject *QuickTestResult::findChild(QObject *parent, const QString &objectName)
{
    return parent ? parent->findChild<QObject*>(objectName) : 0;
}

namespace QTest {
    void qtest_qParseArgs(int argc, char *argv[], bool qml);
};

void QuickTestResult::parseArgs(int argc, char *argv[])
{
    if (!QBenchmarkGlobalData::current)
        QBenchmarkGlobalData::current = &globalBenchmarkData;
    QTest::qtest_qParseArgs(argc, argv, true);
}

void QuickTestResult::setProgramName(const char *name)
{
    if (name) {
        QTestPrivate::parseBlackList();
        QTestPrivate::parseGpuBlackList();
        QTestResult::reset();
    } else if (!name && loggingStarted) {
        QTestResult::setCurrentTestObject(globalProgramName);
        QTestLog::stopLogging();
        QTestResult::setCurrentTestObject(0);
    }
    globalProgramName = name;
    QTestResult::setCurrentTestObject(globalProgramName);
}

void QuickTestResult::setCurrentAppname(const char *appname)
{
    QTestResult::setCurrentAppName(appname);
}

int QuickTestResult::exitCode()
{
#if defined(QTEST_NOEXITCODE)
    return 0;
#else
    // make sure our exit code is never going above 127
    // since that could wrap and indicate 0 test fails
    return qMin(QTestLog::failCount(), 127);
#endif
}

#include "quicktestresult.moc"

QT_END_NAMESPACE
