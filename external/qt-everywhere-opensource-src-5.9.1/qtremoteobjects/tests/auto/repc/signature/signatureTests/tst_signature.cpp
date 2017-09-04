/****************************************************************************
**
** Copyright (C) 2017 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
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
#include <QMetaType>
#include <QProcess>
#include <QStandardPaths>

namespace {

QString findExecutable(const QString &executableName, const QStringList &paths)
{
    const auto path = QStandardPaths::findExecutable(executableName, paths);
    if (!path.isEmpty()) {
        return path;
    }

    qWarning() << "Could not find executable:" << executableName << "in any of" << paths;
    return QString();
}

}
typedef QLatin1String _;
class tst_Signature: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules("qt.remoteobjects.warning=false");
    }

    void cleanup()
    {
        // wait for delivery of RemoveObject events to the source
        QTest::qWait(200);
    }

    void testRun()
    {
        qDebug() << "Starting signatureServer process";
        QProcess serverProc;
        serverProc.setProcessChannelMode(QProcess::ForwardedChannels);
        serverProc.start(findExecutable("signatureServer", {
            QCoreApplication::applicationDirPath() + "/../signatureServer/"
        }));
        QVERIFY(serverProc.waitForStarted());

        // wait for server start
        QTest::qWait(200);

        QStringList tests;
        tests << _("differentGlobalEnum")
              << _("differentClassEnum")
              << _("differentPropertyCount")
              << _("differentPropertyType")
              << _("scrambledProperties")
              << _("differentSlotCount")
              << _("differentSlotType")
              << _("differentSlotParamCount")
              << _("differentSlotParamType")
              << _("scrambledSlots")
              << _("differentSignalCount")
              << _("differentSignalParamCount")
              << _("differentSignalParamType")
              << _("scrambledSignals")
              << _("state")
              << _("matchAndQuit"); // matchAndQuit should be the last one

        foreach (auto test, tests) {
            qDebug() << "Starting" << test << "process";
            QProcess testProc;
            testProc.setProcessChannelMode(QProcess::ForwardedChannels);
            testProc.start(findExecutable(test, {
                QCoreApplication::applicationDirPath() + _("/../") + test + _("/")
            }));
            QVERIFY(testProc.waitForStarted());
            QVERIFY(testProc.waitForFinished());
            QCOMPARE(testProc.exitCode(), 0);
        }

        QVERIFY(serverProc.waitForFinished());
        QCOMPARE(serverProc.exitCode(), 0);
    }
};

QTEST_MAIN(tst_Signature)

#include "tst_signature.moc"
