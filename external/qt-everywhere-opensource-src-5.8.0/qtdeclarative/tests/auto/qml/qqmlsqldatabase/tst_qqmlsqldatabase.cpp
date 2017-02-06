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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/private/qquicktext_p.h>
#include <private/qqmlengine_p.h>
#include <QtCore/qcryptographichash.h>
/*
#include <QtWebKit/qwebpage.h>
#include <QtWebKit/qwebframe.h>
#include <QtWebKit/qwebdatabase.h>
#include <QtWebKit/qwebsecurityorigin.h>
*/
#include <QtSql/qsqldatabase.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include "../../shared/util.h"

class tst_qqmlsqldatabase : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlsqldatabase()
    {
        qApp->setApplicationName("tst_qqmlsqldatabase");
        qApp->setOrganizationName("QtProject");
        qApp->setOrganizationDomain("www.qt-project.org");
        engine = new QQmlEngine;
    }

    ~tst_qqmlsqldatabase()
    {
        delete engine;
    }

private slots:
    void initTestCase();

    void checkDatabasePath();

    void testQml_data();
    void testQml();
    void testQml_cleanopen_data();
    void testQml_cleanopen();
    void totalDatabases();

    void cleanupTestCase();

private:
    QString dbDir() const;
    QQmlEngine *engine;
};

void removeRecursive(const QString& dirname)
{
    QDir dir(dirname);
    QFileInfoList entries(dir.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot));
    for (int i = 0; i < entries.count(); ++i)
        if (entries[i].isDir())
            removeRecursive(entries[i].filePath());
        else
            dir.remove(entries[i].fileName());
    QDir().rmdir(dirname);
}

void tst_qqmlsqldatabase::initTestCase()
{
    if (engine->offlineStoragePath().isEmpty())
        QSKIP("offlineStoragePath is empty, skip this test.");
    QQmlDataTest::initTestCase();
    removeRecursive(dbDir());
    QDir().mkpath(dbDir());
}

void tst_qqmlsqldatabase::cleanupTestCase()
{
    if (engine->offlineStoragePath().isEmpty())
        QSKIP("offlineStoragePath is empty, skip this test.");
    removeRecursive(dbDir());
}

QString tst_qqmlsqldatabase::dbDir() const
{
    static QString tmpd = QDir::tempPath()+"/tst_qqmlsqldatabase_output-"
        + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddhhmmss"));
    return tmpd;
}

void tst_qqmlsqldatabase::checkDatabasePath()
{
    if (engine->offlineStoragePath().isEmpty())
        QSKIP("offlineStoragePath is empty, skip this test.");

    // Check default storage path (we can't use it since we don't want to mess with user's data)
    QVERIFY(engine->offlineStoragePath().contains("tst_qqmlsqldatabase"));
    QVERIFY(engine->offlineStoragePath().contains("OfflineStorage"));
}

static const int total_databases_created_by_tests = 13;
void tst_qqmlsqldatabase::testQml_data()
{
    QTest::addColumn<QString>("jsfile"); // The input file

    // Each test should use a newly named DB to avoid inter-test dependencies
    QTest::newRow("creation") << "creation.js";
    QTest::newRow("creation-a") << "creation-a.js";
    QTest::newRow("creation") << "creation.js";
    QTest::newRow("error-creation") << "error-creation.js"; // re-uses above DB
    QTest::newRow("changeversion") << "changeversion.js";
    QTest::newRow("readonly") << "readonly.js";
    QTest::newRow("readonly-error") << "readonly-error.js";
    QTest::newRow("selection") << "selection.js";
    QTest::newRow("selection-bindnames") << "selection-bindnames.js";
    QTest::newRow("iteration") << "iteration.js";
    QTest::newRow("iteration-forwardonly") << "iteration-forwardonly.js";
    QTest::newRow("error-a") << "error-a.js";
    QTest::newRow("error-notransaction") << "error-notransaction.js";
    QTest::newRow("error-outsidetransaction") << "error-outsidetransaction.js"; // reuse above
    QTest::newRow("reopen1") << "reopen1.js";
    QTest::newRow("reopen2") << "reopen2.js"; // re-uses above DB
    QTest::newRow("null-values") << "nullvalues.js";

    // If you add a test, you should usually use a new database in the
    // test - in which case increment total_databases_created_by_tests above.
}

/*
class QWebPageWithJavaScriptConsoleMessages : public QWebPage {
public:
    void javaScriptConsoleMessage(const QString& message, int lineNumber, const QString& sourceID)
    {
        qWarning() << sourceID << ":" << lineNumber << ":" << message;
    }
};

void tst_qqmlsqldatabase::validateAgainstWebkit()
{
    // Validates tests against WebKit (HTML5) support.
    //
    QFETCH(QString, jsfile);
    QFETCH(QString, result);
    QFETCH(int, databases);

    QFile f(jsfile);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QString js=f.readAll();

    QWebPageWithJavaScriptConsoleMessages webpage;
    webpage.settings()->setOfflineStoragePath(dbDir());
    webpage.settings()->setAttribute(QWebSettings::OfflineStorageDatabaseEnabled, true);

    QEXPECT_FAIL("","WebKit doesn't support openDatabaseSync yet", Continue);
    QCOMPARE(webpage.mainFrame()->evaluateJavaScript(js).toString(),result);

    QTest::qWait(100); // WebKit crashes if you quit it too fast

    QWebSecurityOrigin origin = webpage.mainFrame()->securityOrigin();
    QList<QWebDatabase> dbs = origin.databases();
    QCOMPARE(dbs.count(), databases);
}
*/

void tst_qqmlsqldatabase::testQml()
{
    if (engine->offlineStoragePath().isEmpty())
        QSKIP("offlineStoragePath is empty, skip this test.");

    // Tests QML SQL Database support with tests
    // that have been validated against Webkit.
    //
    QFETCH(QString, jsfile);

    QString qml=
        "import QtQuick 2.0\n"
        "import \""+jsfile+"\" as JS\n"
        "Text { text: JS.test() }";

    engine->setOfflineStoragePath(dbDir());
    QQmlComponent component(engine);
    component.setData(qml.toUtf8(), testFileUrl("empty.qml")); // just a file for relative local imports
    QVERIFY(!component.isError());
    QQuickText *text = qobject_cast<QQuickText*>(component.create());
    QVERIFY(text != 0);
    QCOMPARE(text->text(),QString("passed"));
}

void tst_qqmlsqldatabase::testQml_cleanopen_data()
{
    QTest::addColumn<QString>("jsfile"); // The input file
    QTest::newRow("reopen1") << "reopen1.js";
    QTest::newRow("reopen2") << "reopen2.js";
    QTest::newRow("error-creation") << "error-creation.js"; // re-uses creation DB
}

void tst_qqmlsqldatabase::testQml_cleanopen()
{
    if (engine->offlineStoragePath().isEmpty())
        QSKIP("offlineStoragePath is empty, skip this test.");

    // Same as testQml, but clean connections between tests,
    // making it more like the tests are running in new processes.
    testQml();

    engine->collectGarbage();

    foreach (QString dbname, QSqlDatabase::connectionNames()) {
        QSqlDatabase::removeDatabase(dbname);
    }
}

void tst_qqmlsqldatabase::totalDatabases()
{
    if (engine->offlineStoragePath().isEmpty())
        QSKIP("offlineStoragePath is empty, skip this test.");

    QCOMPARE(QDir(dbDir()+"/Databases").entryInfoList(QDir::Files|QDir::NoDotAndDotDot).count(), total_databases_created_by_tests*2);
}

QTEST_MAIN(tst_qqmlsqldatabase)

#include "tst_qqmlsqldatabase.moc"
