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


#include "abstracttestsuite.h"
#include <QtTest/QtTest>

#include <QtScript>

struct TestRecord
{
    TestRecord() : lineNumber(-1) { }
    TestRecord(const QString &desc,
               bool pass,
               const QString &act,
               const QString &exp,
               const QString &fn, int ln)
        : description(desc), passed(pass),
          actual(act), expected(exp),
          fileName(fn), lineNumber(ln)
        { }
    TestRecord(const QString &skipReason, const QString &fn)
        : description(skipReason), actual("QSKIP"),
          fileName(fn), lineNumber(-1)
        { }
    QString description;
    bool passed;
    QString actual;
    QString expected;
    QString fileName;
    int lineNumber;
};

Q_DECLARE_METATYPE(TestRecord)

struct FailureItem
{
    enum Action {
        ExpectFail,
        Skip
    };
    FailureItem(Action act, const QRegExp &rx, const QString &desc, const QString &msg)
        : action(act), pathRegExp(rx), description(desc), message(msg)
        { }

    Action action;
    QRegExp pathRegExp;
    QString description;
    QString message;
};

class tst_QScriptJSTestSuite : public AbstractTestSuite
{

public:
    tst_QScriptJSTestSuite();
    virtual ~tst_QScriptJSTestSuite();

protected:
    virtual void configData(TestConfig::Mode mode, const QStringList &parts);
    virtual void writeSkipConfigFile(QTextStream &);
    virtual void writeExpectFailConfigFile(QTextStream &);
    virtual void runTestFunction(int testIndex);

private:
    void addExpectedFailure(const QString &fileName, const QString &description, const QString &message);
    void addExpectedFailure(const QRegExp &path, const QString &description, const QString &message);
    void addSkip(const QString &fileName, const QString &description, const QString &message);
    void addSkip(const QRegExp &path, const QString &description, const QString &message);
    bool isExpectedFailure(const QString &fileName, const QString &description,
                           QString *message, FailureItem::Action *action) const;
    void addFileExclusion(const QString &fileName, const QString &message);
    void addFileExclusion(const QRegExp &rx, const QString &message);
    bool isExcludedFile(const QString &fileName, QString *message) const;

    QList<QString> subSuitePaths;
    QList<FailureItem> expectedFailures;
    QList<QPair<QRegExp, QString> > fileExclusions;
};

static QScriptValue qscript_void(QScriptContext *, QScriptEngine *eng)
{
    return eng->undefinedValue();
}

static QScriptValue qscript_quit(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError("Test quit");
}

static QString optionsToString(int options)
{
    QSet<QString> set;
    if (options & 1)
        set.insert("strict");
    if (options & 2)
        set.insert("werror");
    if (options & 4)
        set.insert("atline");
    if (options & 8)
        set.insert("xml");
    return QStringList(set.values()).join(",");
}

static QScriptValue qscript_options(QScriptContext *ctx, QScriptEngine *)
{
    static QHash<QString, int> stringToFlagHash;
    if (stringToFlagHash.isEmpty()) {
        stringToFlagHash["strict"] = 1;
        stringToFlagHash["werror"] = 2;
        stringToFlagHash["atline"] = 4;
        stringToFlagHash["xml"] = 8;
    }
    QScriptValue callee = ctx->callee();
    int opts = callee.data().toInt32();
    QString result = optionsToString(opts);
    for (int i = 0; i < ctx->argumentCount(); ++i)
        opts |= stringToFlagHash.value(ctx->argument(0).toString());
    callee.setData(opts);
    return result;
}

static QScriptValue qscript_TestCase(QScriptContext *ctx, QScriptEngine *eng)
{
    QScriptValue origTestCaseCtor = ctx->callee().data();
    QScriptValue kase = ctx->thisObject();
    QScriptValue ret = origTestCaseCtor.call(kase, ctx->argumentsObject());
    QScriptContextInfo info(ctx->parentContext());
    kase.setProperty("__lineNumber__", QScriptValue(eng, info.lineNumber()));
    return ret;
}

void tst_QScriptJSTestSuite::runTestFunction(int testIndex)
{
    if (!(testIndex & 1)) {
        // data
        QTest::addColumn<TestRecord>("record");
        bool hasData = false;

        QString testsShellPath = testsDir.absoluteFilePath("shell.js");
        QString testsShellContents = readFile(testsShellPath);

        QDir subSuiteDir(subSuitePaths.at(testIndex / 2));
        QString subSuiteShellPath = subSuiteDir.absoluteFilePath("shell.js");
        QString subSuiteShellContents = readFile(subSuiteShellPath);

        QDir testSuiteDir(subSuiteDir);
        testSuiteDir.cdUp();
        QString suiteJsrefPath = testSuiteDir.absoluteFilePath("jsref.js");
        QString suiteJsrefContents = readFile(suiteJsrefPath);
        QString suiteShellPath = testSuiteDir.absoluteFilePath("shell.js");
        QString suiteShellContents = readFile(suiteShellPath);

        const QFileInfoList testFileInfos = subSuiteDir.entryInfoList(QStringList() << "*.js", QDir::Files);
        for (const QFileInfo &tfi : testFileInfos) {
            if ((tfi.fileName() == "shell.js") || (tfi.fileName() == "browser.js"))
                continue;

            QString abspath = tfi.absoluteFilePath();
            QString relpath = testsDir.relativeFilePath(abspath);
            QString excludeMessage;
            if (isExcludedFile(relpath, &excludeMessage)) {
                QTest::newRow(relpath.toLatin1()) << TestRecord(excludeMessage, relpath);
                continue;
            }

            QScriptEngine eng;
            QScriptValue global = eng.globalObject();
            global.setProperty("print", eng.newFunction(qscript_void));
            global.setProperty("quit", eng.newFunction(qscript_quit));
            global.setProperty("options", eng.newFunction(qscript_options));

            eng.evaluate(testsShellContents, testsShellPath);
            if (eng.hasUncaughtException()) {
                QStringList bt = eng.uncaughtExceptionBacktrace();
                QString err = eng.uncaughtException().toString();
                qWarning("%s\n%s", qPrintable(err), qPrintable(bt.join("\n")));
                break;
            }

            eng.evaluate(suiteJsrefContents, suiteJsrefPath);
            if (eng.hasUncaughtException()) {
                QStringList bt = eng.uncaughtExceptionBacktrace();
                QString err = eng.uncaughtException().toString();
                qWarning("%s\n%s", qPrintable(err), qPrintable(bt.join("\n")));
                break;
            }

            eng.evaluate(suiteShellContents, suiteShellPath);
            if (eng.hasUncaughtException()) {
                QStringList bt = eng.uncaughtExceptionBacktrace();
                QString err = eng.uncaughtException().toString();
                qWarning("%s\n%s", qPrintable(err), qPrintable(bt.join("\n")));
                break;
            }

            eng.evaluate(subSuiteShellContents, subSuiteShellPath);
            if (eng.hasUncaughtException()) {
                QStringList bt = eng.uncaughtExceptionBacktrace();
                QString err = eng.uncaughtException().toString();
                qWarning("%s\n%s", qPrintable(err), qPrintable(bt.join("\n")));
                break;
            }

            QScriptValue origTestCaseCtor = global.property("TestCase");
            QScriptValue myTestCaseCtor = eng.newFunction(qscript_TestCase);
            myTestCaseCtor.setData(origTestCaseCtor);
            global.setProperty("TestCase", myTestCaseCtor);

            global.setProperty("gTestfile", tfi.fileName());
            global.setProperty("gTestsuite", testSuiteDir.dirName());
            global.setProperty("gTestsubsuite", subSuiteDir.dirName());
            QString testFileContents = readFile(abspath);
//                qDebug() << relpath;
            eng.evaluate(testFileContents, abspath);
            if (eng.hasUncaughtException() && !relpath.endsWith("-n.js")) {
                QStringList bt = eng.uncaughtExceptionBacktrace();
                QString err = eng.uncaughtException().toString();
                qWarning("%s\n%s\n", qPrintable(err), qPrintable(bt.join("\n")));
                continue;
            }

            QScriptValue testcases = global.property("testcases");
            if (!testcases.isArray())
                testcases = global.property("gTestcases");
            int count = testcases.property("length").toInt32();
            if (count == 0)
                continue;

            hasData = true;
            QString title = global.property("TITLE").toString();
            for (int i = 0; i < count; ++i) {
                QScriptValue kase = testcases.property(i);
                QString description = kase.property("description").toString();
                QScriptValue expect = kase.property("expect");
                QScriptValue actual = kase.property("actual");
                bool passed = kase.property("passed").toBoolean();
                int lineNumber = kase.property("__lineNumber__").toInt32();

                TestRecord rec(description, passed,
                               actual.toString(), expect.toString(),
                               relpath, lineNumber);

                QTest::newRow(description.toLatin1()) << rec;
            }
        }
        if (!hasData)
            QTest::newRow("") << TestRecord(); // dummy
    } else {
        QFETCH(TestRecord, record);
        if ((record.lineNumber == -1) && (record.actual == "QSKIP")) {
            QTest::qSkip(record.description.toLatin1(), record.fileName.toLatin1(), -1);
        } else {
            QString msg;
            FailureItem::Action failAct;
            bool expectFail = isExpectedFailure(record.fileName, record.description, &msg, &failAct);
            if (expectFail) {
                switch (failAct) {
                case FailureItem::ExpectFail:
                    QTest::qExpectFail("", msg.toLatin1(),
                                       QTest::Continue, record.fileName.toLatin1(),
                                       record.lineNumber);
                    break;
                case FailureItem::Skip:
                    QTest::qSkip(msg.toLatin1(), record.fileName.toLatin1(), record.lineNumber);
                    break;
                }
            }
            if (!expectFail || (failAct == FailureItem::ExpectFail)) {
                if (!record.passed) {
                    if (!expectFail && shouldGenerateExpectedFailures) {
                        addExpectedFailure(record.fileName,
                                           record.description,
                                           QString());
                    }
                    QTest::qCompare(record.actual, record.expected, "actual", "expect",
                                    record.fileName.toLatin1(), record.lineNumber);
                } else {
                    QTest::qCompare(record.actual, record.actual, "actual", "expect",
                                    record.fileName.toLatin1(), record.lineNumber);
                }
            }
        }
    }
}

tst_QScriptJSTestSuite::tst_QScriptJSTestSuite()
    : AbstractTestSuite("tst_QScriptJsTestSuite",
                        QFINDTESTDATA("tests"),
                        ":/")
{
// don't execute any tests on slow machines
#if !defined(Q_OS_IRIX)
    // do all the test suites
    const QFileInfoList testSuiteDirInfos = testsDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &tsdi : testSuiteDirInfos) {
        QDir testSuiteDir(tsdi.absoluteFilePath());
        // do all the dirs in the test suite
        const QFileInfoList subSuiteDirInfos = testSuiteDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &ssdi : subSuiteDirInfos) {
            subSuitePaths.append(ssdi.absoluteFilePath());
            QString function = QString::fromLatin1("%0/%1")
                               .arg(testSuiteDir.dirName()).arg(ssdi.fileName());
            addTestFunction(function, CreateDataFunction);
        }
    }
#endif

    finalizeMetaObject();
}

tst_QScriptJSTestSuite::~tst_QScriptJSTestSuite()
{
}

void tst_QScriptJSTestSuite::configData(TestConfig::Mode mode, const QStringList &parts)
{
    switch (mode) {
    case TestConfig::Skip:
        addFileExclusion(parts.at(0), parts.value(1));
        break;

    case TestConfig::ExpectFail:
        addExpectedFailure(parts.at(0), parts.value(1), parts.value(2));
        break;
    }
}

void tst_QScriptJSTestSuite::writeSkipConfigFile(QTextStream &stream)
{
    stream << QString::fromLatin1("# testcase | message") << endl;
}

void tst_QScriptJSTestSuite::writeExpectFailConfigFile(QTextStream &stream)
{
    stream << QString::fromLatin1("# testcase | description | message") << endl;
    for (int i = 0; i < expectedFailures.size(); ++i) {
        const FailureItem &fail = expectedFailures.at(i);
        if (fail.pathRegExp.pattern().isEmpty())
            continue;
        stream << QString::fromLatin1("%0 | %1")
            .arg(fail.pathRegExp.pattern())
            .arg(escape(fail.description));
        if (!fail.message.isEmpty())
            stream << QString::fromLatin1(" | %0").arg(escape(fail.message));
        stream << endl;
    }
}

void tst_QScriptJSTestSuite::addExpectedFailure(const QRegExp &path, const QString &description, const QString &message)
{
    expectedFailures.append(FailureItem(FailureItem::ExpectFail, path, description, message));
}

void tst_QScriptJSTestSuite::addExpectedFailure(const QString &fileName, const QString &description, const QString &message)
{
    expectedFailures.append(FailureItem(FailureItem::ExpectFail, QRegExp(fileName), description, message));
}

void tst_QScriptJSTestSuite::addSkip(const QRegExp &path, const QString &description, const QString &message)
{
    expectedFailures.append(FailureItem(FailureItem::Skip, path, description, message));
}

void tst_QScriptJSTestSuite::addSkip(const QString &fileName, const QString &description, const QString &message)
{
    expectedFailures.append(FailureItem(FailureItem::Skip, QRegExp(fileName), description, message));
}

bool tst_QScriptJSTestSuite::isExpectedFailure(const QString &fileName, const QString &description,
                                  QString *message, FailureItem::Action *action) const
{
    for (int i = 0; i < expectedFailures.size(); ++i) {
        QRegExp pathRegExp = expectedFailures.at(i).pathRegExp;
        if (pathRegExp.indexIn(fileName) != -1) {
            if (description == expectedFailures.at(i).description) {
                if (message)
                    *message = expectedFailures.at(i).message;
                if (action)
                    *action = expectedFailures.at(i).action;
                return true;
            }
        }
    }
    return false;
}

void tst_QScriptJSTestSuite::addFileExclusion(const QString &fileName, const QString &message)
{
    fileExclusions.append(qMakePair(QRegExp(fileName), message));
}

void tst_QScriptJSTestSuite::addFileExclusion(const QRegExp &rx, const QString &message)
{
    fileExclusions.append(qMakePair(rx, message));
}

bool tst_QScriptJSTestSuite::isExcludedFile(const QString &fileName, QString *message) const
{
    for (int i = 0; i < fileExclusions.size(); ++i) {
        QRegExp copy = fileExclusions.at(i).first;
        if (copy.indexIn(fileName) != -1) {
            if (message)
                *message = fileExclusions.at(i).second;
            return true;
    }
    }
    return false;
}

QTEST_MAIN(tst_QScriptJSTestSuite)
