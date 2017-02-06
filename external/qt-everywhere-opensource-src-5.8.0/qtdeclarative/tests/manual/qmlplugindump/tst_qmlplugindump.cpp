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

#include <qtest.h>
#include <QLibraryInfo>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <QDebug>

#include <cstdlib>
#include <algorithm>


// System dependent definitions

#if defined(Q_OS_WIN)

static const char* systemMakeProgram = "jom";
static const char* systemQmlplugindumpProgram = "qmlplugindump.exe";

static const QString libname(const QString &name)
{
    return name + QLatin1String(".dll");
}

#elif defined(Q_OS_DARWIN)

static const char* systemMakeProgram = "make";
static const char* systemQmlplugindumpProgram = "qmlplugindump";

static const QString libname(const QString &name)
{
    return QLatin1String("lib") + name + QLatin1String(".dynlib");
}

#else

static const char* systemMakeProgram = "make";
static const char* systemQmlplugindumpProgram = "qmlplugindump";

static const QString libname(const QString &name)
{
    return QLatin1String("lib") + name + QLatin1String(".so");
}

#endif


// Utilities


// Functor for matching a list of regular expression on a buffer.
//
class RegexpsChecker
{
public:
    RegexpsChecker(const QStringList &expected) : m_expected(expected) {}
    bool operator()(const QByteArray &buffer) const
    {
        QRegularExpression re;
        QRegularExpressionMatch m;
        for (const QString &e : m_expected) {
            re.setPattern(e);
            m = re.match(QString::fromLatin1(buffer));
            if (!m.hasMatch()) {
                qWarning() << "Pattern not found: " << e;
                return false;
            }
        }
        return true;
    }
private:
    const QStringList m_expected;
};


// Run an external process in given path and with given arguments, if presents.
// Optionally run a regexp check on the process standard output.
//
bool run(const QString &path, const QString &cmd, const QStringList &args=QStringList(),
         const RegexpsChecker *checker=0) {
    QProcess process;
    process.setWorkingDirectory(path);
    process.start(cmd, args);
    process.waitForFinished();
    if (process.error() == QProcess::FailedToStart) {
        qWarning() << cmd << args << "failed to start.";
        return false;
    }
    if (process.error() == QProcess::Crashed) {
        qWarning() << cmd << args << "crashed.";
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        qWarning() << cmd << args << "exited with code: " << process.exitCode();
        return false;
    }
    if (checker)
        (*checker)(process.readAllStandardOutput());
    return true;
}


// Test Definition
//
// A test is defined by id, project, version and expected patterns that should
// match the output of qmlplugindump.
//
class Test
{
public:
    Test(const QString &testId) : id(testId) {}
    Test(const QString &testId, const QString &prjName, const QString &prjVersion,
         const QStringList &expectedResult) :
        id(testId), project(prjName), version(prjVersion), expected(expectedResult) {}
    QString id;
    QString project;
    QString version;
    QStringList expected;
    bool isNull() const
    {
        return project.isEmpty() || version.isEmpty();
    }
    friend bool operator<(const Test &t1, const Test &t2) { return t1.id < t2.id; }
};


// Create test from a json document.
//
// The json document must contains these fields:
//
//     project: string
//     version: string
//     expected: [string*]
//
// qmlplugindumper will be invoked with these arguments:
//
//     qmlplugindumper tests.dumper.<PROJECT> <VERSION>
//
// PROJECT is the name of the sample project.  It is used as a project name
// and as the last components of the URI: tests.dumper.<PROJECT>; the project
// path must be ./test/dumper/<PROJECT>.
//
// EXPECTED is a list of regular expression patterns that must match the
// plugin.qmltypes produced by qmlplugindump.
//
Test createTest(const QString &id, const QJsonObject &def)
{
    QJsonValue project = def.value(QLatin1String("project"));
    QJsonValue version = def.value(QLatin1String("version"));
    QJsonValue expected = def.value(QLatin1String("expected"));
    if (!project.isString() || !version.isString() || !expected.isArray()){
        qWarning() << "Wrong definition for test: " << id << ".";
        return Test(id);
    }
    QStringList patterns;
    const auto expectedArray = expected.toArray();
    for (const QJsonValue &x : expectedArray) {
        if (!x.isString()) {
            qWarning() << "Wrong definition for test: " << id << ".";
            return Test(id);
        } else {
            patterns << x.toString();
        }
    }
    return Test(id, project.toString(), version.toString(), patterns);
}

// Read a test definition from a file.
//
// The file must define a json document that `createTest' can understand.
//
Test readTestDefinition(const QFileInfo &file)
{
    const QString path = file.filePath();
    const QString id = file.baseName();
    QFile fd(path);
    if (fd.open(QIODevice::ReadOnly)) {
        QTextStream in(&fd);
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(in.readAll().toLatin1(), &err);
        fd.close();
        if (err.error != QJsonParseError::NoError) {
            qWarning() << "Parse error in test \"" << id << "\":" << err.errorString();
            return Test(id);
        }
        QJsonObject obj = doc.object();
        return createTest(id, obj);
    }
    qWarning() << "Cannot open " << path << ".";
    return Test(id);
}

// Read all files in `path' as test definitions.
//
// Returns a list of tests sorted lexicographically.
//
QList<Test> readAllTestDefinitions(const QString &path)
{
    QList<Test> samples;
    QDirIterator it(path, QDir::Files);
    while (it.hasNext()) {
        it.next();
        samples << readTestDefinition(it.fileInfo());
    }
    std::sort(samples.begin(), samples.end());
    return samples;
}


// TEST SUITE

class tst_qmlplugindump : public QObject
{
    Q_OBJECT
public:
    tst_qmlplugindump();

private slots:
    void initTestCase();
    void builtins();
    void plugin_data();
    void plugin();
    void cleanupTestCase();

private:
    static const char *prefix;
    QString dumper;
    QList<Test> tests;
    QString samplePath(const QString &name);
    bool compileSample(const QString &name);
    bool cleanUpSample(const QString &name);
};

const char *tst_qmlplugindump::prefix = "tests.dumper.";

tst_qmlplugindump::tst_qmlplugindump()
{
}

QString tst_qmlplugindump::samplePath(const QString &name)
{
    return QCoreApplication::applicationDirPath()
        + QLatin1Char('/')
        + QString(QLatin1String(prefix)).replace(QRegularExpression(QLatin1String("\\.")),
                QLatin1String("/")).append(name);
}

bool tst_qmlplugindump::compileSample(const QString &name)
{
    const QString path = samplePath(name);
    return run(path, QLatin1String("qmake"))
        && run(path, QLatin1String(systemMakeProgram));
}

bool tst_qmlplugindump::cleanUpSample(const QString &name)
{
    const QString path = samplePath(name);
    const QStringList args(QLatin1String("clean"));
    if (!run(path, QLatin1String(systemMakeProgram), args))
        return false;

    const QString libfile = path + QLatin1Char('/') + libname(name);

    if (!QFile::remove(libfile)) {
        qWarning() << "Cannot remove" << libfile << ".";
        return false;
    }
    return true;
}

void tst_qmlplugindump::initTestCase()
{
    dumper = QLibraryInfo::location(QLibraryInfo::BinariesPath);
    tests = readAllTestDefinitions(QCoreApplication::applicationDirPath()
                                   + QLatin1String("/definitions"));

    dumper += QLatin1Char('/') + QLatin1String(systemQmlplugindumpProgram);

    if (!QFileInfo(dumper).exists()) {
        QString message = QString::fromLatin1("qmlplugindump executable not found (looked for %0)")
                .arg(dumper);
        QFAIL(qPrintable(message));
    }
}

void tst_qmlplugindump::builtins()
{
    QStringList args;
    args += QLatin1String("-builtins");
    RegexpsChecker check(QStringList(QLatin1String("Module {")));
    QString cwd = QCoreApplication::applicationDirPath();
    QVERIFY(run(cwd, dumper, args, &check));
}

void tst_qmlplugindump::plugin_data()
{
    QTest::addColumn<QString>("project");
    QTest::addColumn<QString>("version");
    QTest::addColumn<QStringList>("expected");

    for (const Test &t : qAsConst(tests)) {
        if (t.isNull())
            QSKIP("Errors in test definition.");
        QTest::newRow(t.id.toLatin1().data()) << t.project << t.version << t.expected;
    }
}

void tst_qmlplugindump::plugin()
{
    QFETCH(QString, project);
    QFETCH(QString, version);
    QFETCH(QStringList, expected);

    QVERIFY(compileSample(project));

    QStringList args;
    QString url = QLatin1String("tests.dumper.") + project;
    QString cwd = QCoreApplication::applicationDirPath();
    args << QLatin1String("-nonrelocatable") << url << version << cwd;
    RegexpsChecker check(expected);
    QVERIFY(run(cwd, dumper, args, &check));
}

void tst_qmlplugindump::cleanupTestCase()
{
    QSet<const QString> projects;
    for (const Test &t : qAsConst(tests))
        projects.insert(t.project);
    for (const QString &p : qAsConst(projects)) {
        if (!cleanUpSample(p))
            qWarning() << "Error in cleaning up project" << p << ".";
    }
}


QTEST_MAIN(tst_qmlplugindump)

#include "tst_qmlplugindump.moc"
