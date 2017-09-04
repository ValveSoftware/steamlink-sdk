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

#include <QQmlComponent>
#include <QQmlEngine>
#include <QProcess>
#include <QLibraryInfo>
#include <QSysInfo>

class tst_qmlcachegen: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void loadGeneratedFile();
    void translationExpressionSupport();
};

// A wrapper around QQmlComponent to ensure the temporary reference counts
// on the type data as a result of the main thread <> loader thread communication
// are dropped. Regular Synchronous loading will leave us with an event posted
// to the gui thread and an extra refcount that will only be dropped after the
// event delivery. A plain sendPostedEvents() however is insufficient because
// we can't be sure that the event is posted after the constructor finished.
class CleanlyLoadingComponent : public QQmlComponent
{
public:
    CleanlyLoadingComponent(QQmlEngine *engine, const QUrl &url)
        : QQmlComponent(engine, url, QQmlComponent::Asynchronous)
    { waitForLoad(); }
    CleanlyLoadingComponent(QQmlEngine *engine, const QString &fileName)
        : QQmlComponent(engine, fileName, QQmlComponent::Asynchronous)
    { waitForLoad(); }

    void waitForLoad()
    {
        QTRY_VERIFY(status() == QQmlComponent::Ready || status() == QQmlComponent::Error);
    }
};

static bool generateCache(const QString &qmlFileName)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.setProgram(QLibraryInfo::location(QLibraryInfo::BinariesPath) + QDir::separator() + QLatin1String("qmlcachegen"));
    proc.setArguments(QStringList() << (QLatin1String("--target-architecture=") + QSysInfo::buildCpuArchitecture()) << (QLatin1String("--target-abi=") + QSysInfo::buildAbi()) << qmlFileName);
    proc.start();
    if (!proc.waitForFinished())
        return false;
    if (proc.exitStatus() != QProcess::NormalExit)
        return false;
    return proc.exitCode() == 0;
}

void tst_qmlcachegen::initTestCase()
{
    qputenv("QML_FORCE_DISK_CACHE", "1");
}

void tst_qmlcachegen::loadGeneratedFile()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const auto writeTempFile = [&tempDir](const QString &fileName, const char *contents) {
        QFile f(tempDir.path() + '/' + fileName);
        const bool ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        Q_ASSERT(ok);
        f.write(contents);
        return f.fileName();
    };

    const QString testFilePath = writeTempFile("test.qml", "import QtQml 2.0\n"
                                                           "QtObject {\n"
                                                           "    property int value: Math.min(100, 42);\n"
                                                           "}");

    QVERIFY(generateCache(testFilePath));

    const QString cacheFilePath = testFilePath + QLatin1Char('c');
    QVERIFY(QFile::exists(cacheFilePath));
    QVERIFY(QFile::remove(testFilePath));

    QQmlEngine engine;
    CleanlyLoadingComponent component(&engine, QUrl::fromLocalFile(testFilePath));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(!obj.isNull());
    QCOMPARE(obj->property("value").toInt(), 42);
}

void tst_qmlcachegen::translationExpressionSupport()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const auto writeTempFile = [&tempDir](const QString &fileName, const char *contents) {
        QFile f(tempDir.path() + '/' + fileName);
        const bool ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        Q_ASSERT(ok);
        f.write(contents);
        return f.fileName();
    };

    const QString testFilePath = writeTempFile("test.qml", "import QtQml.Models 2.2\n"
                                                           "import QtQml 2.2\n"
                                                           "QtObject {\n"
                                                           "    property ListModel model: ListModel {\n"
                                                           "        ListElement {\n"
                                                           "            text: qsTr(\"All\")\n"
                                                           "        }\n"
                                                           "        ListElement {\n"
                                                           "            text: QT_TR_NOOP(\"Ok\")\n"
                                                           "        }\n"
                                                           "    }\n"
                                                           "    property string text: model.get(0).text + \" \" + model.get(1).text\n"
                                                           "}");


    QVERIFY(generateCache(testFilePath));

    const QString cacheFilePath = testFilePath + QLatin1Char('c');
    QVERIFY(QFile::exists(cacheFilePath));
    QVERIFY(QFile::remove(testFilePath));

    QQmlEngine engine;
    CleanlyLoadingComponent component(&engine, QUrl::fromLocalFile(testFilePath));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(!obj.isNull());
    QCOMPARE(obj->property("text").toString(), QString("All Ok"));
}

QTEST_GUILESS_MAIN(tst_qmlcachegen)

#include "tst_qmlcachegen.moc"
