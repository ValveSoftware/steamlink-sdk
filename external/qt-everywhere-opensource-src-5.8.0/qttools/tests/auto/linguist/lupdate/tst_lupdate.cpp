/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#if CHECK_SIMTEXTH
#include "../shared/simtexth.h"
#endif

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QByteArray>

#include <QtTest/QtTest>

class tst_lupdate : public QObject
{
    Q_OBJECT
public:
    tst_lupdate();

private slots:
    void good_data();
    void good();
#if CHECK_SIMTEXTH
    void simtexth();
    void simtexth_data();
#endif

private:
    QString m_cmdLupdate;
    QString m_basePath;

    void doCompare(QStringList actual, const QString &expectedFn, bool err);
    void doCompare(const QString &actualFn, const QString &expectedFn, bool err);
};


tst_lupdate::tst_lupdate()
{
    QString binPath = QLibraryInfo::location(QLibraryInfo::BinariesPath);
    m_cmdLupdate = binPath + QLatin1String("/lupdate");
    m_basePath = QFINDTESTDATA("testdata/");
}

static bool prepareMatch(const QString &expect, QString *tmpl, int *require, int *accept)
{
    if (expect.startsWith(QLatin1Char('\\'))) {
        *tmpl = expect.mid(1);
        *require = *accept = 1;
    } else if (expect.startsWith(QLatin1Char('?'))) {
        *tmpl = expect.mid(1);
        *require = 0;
        *accept = 1;
    } else if (expect.startsWith(QLatin1Char('*'))) {
        *tmpl = expect.mid(1);
        *require = 0;
        *accept = INT_MAX;
    } else if (expect.startsWith(QLatin1Char('+'))) {
        *tmpl = expect.mid(1);
        *require = 1;
        *accept = INT_MAX;
    } else if (expect.startsWith(QLatin1Char('{'))) {
        int brc = expect.indexOf(QLatin1Char('}'), 1);
        if (brc < 0)
            return false;
        *tmpl = expect.mid(brc + 1);
        QString sub = expect.mid(1, brc - 1);
        int com = sub.indexOf(QLatin1Char(','));
        bool ok;
        if (com < 0) {
            *require = *accept = sub.toInt(&ok);
            return ok;
        } else {
            *require = sub.left(com).toInt();
            *accept = sub.mid(com + 1).toInt(&ok);
            if (!ok)
                *accept = INT_MAX;
            return *accept >= *require;
        }
    } else {
        *tmpl = expect;
        *require = *accept = 1;
    }
    return true;
}

void tst_lupdate::doCompare(QStringList actual, const QString &expectedFn, bool err)
{
    QFile file(expectedFn);
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(expectedFn));
    QStringList expected = QString(file.readAll()).split('\n');

    for (int i = actual.size() - 1; i >= 0; --i) {
        if (actual.at(i).startsWith(QLatin1String("Info: creating stash file ")))
            actual.removeAt(i);
    }

    int ei = 0, ai = 0, em = expected.size(), am = actual.size();
    int oei = 0, oai = 0, oem = em, oam = am;
    int require = 0, accept = 0;
    QString tmpl;
    forever {
        if (!accept) {
            oei = ei, oai = ai;
            if (ei == em) {
                if (ai == am)
                    return;
                break;
            }
            if (!prepareMatch(expected.at(ei++), &tmpl, &require, &accept))
                QFAIL(qPrintable(QString("Malformed expected %1 at %3:%2")
                                 .arg(err ? "output" : "result").arg(ei).arg(expectedFn)));
        }
        if (ai == am) {
            if (require <= 0) {
                accept = 0;
                continue;
            }
            break;
        }
        if (err ? !QRegExp(tmpl).exactMatch(actual.at(ai)) : (actual.at(ai) != tmpl)) {
            if (require <= 0) {
                accept = 0;
                continue;
            }
            ei--;
            require = accept = 0;
            forever {
                if (!accept) {
                    oem = em, oam = am;
                    if (ei == em)
                        break;
                    if (!prepareMatch(expected.at(--em), &tmpl, &require, &accept))
                        QFAIL(qPrintable(QString("Malformed expected %1 at %3:%2")
                                         .arg(err ? "output" : "result")
                                         .arg(em + 1).arg(expectedFn)));
                }
                if (ai == am || (err ? !QRegExp(tmpl).exactMatch(actual.at(am - 1)) :
                                       (actual.at(am - 1) != tmpl))) {
                    if (require <= 0) {
                        accept = 0;
                        continue;
                    }
                    break;
                }
                accept--;
                require--;
                am--;
            }
            break;
        }
        accept--;
        require--;
        ai++;
    }
    QByteArray diff;
    for (int j = qMax(0, oai - 3); j < oai; j++)
        diff += actual.at(j) + '\n';
    diff += "<<<<<<< got\n";
    for (int j = oai; j < oam; j++) {
        diff += actual.at(j) + '\n';
        if (j >= oai + 5) {
            diff += "...\n";
            break;
        }
    }
    diff += "=========\n";
    for (int j = oei; j < oem; j++) {
        diff += expected.at(j) + '\n';
        if (j >= oei + 5) {
            diff += "...\n";
            break;
        }
    }
    diff += ">>>>>>> expected\n";
    for (int j = oam; j < qMin(oam + 3, actual.size()); j++)
        diff += actual.at(j) + '\n';
    QFAIL(qPrintable((err ? "Output for " : "Result for ") + expectedFn + " does not meet expectations:\n" + diff));
}

void tst_lupdate::doCompare(const QString &actualFn, const QString &expectedFn, bool err)
{
    QFile afile(actualFn);
    QVERIFY2(afile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(actualFn));
    QStringList actual = QString(afile.readAll()).split('\n');

    doCompare(actual, expectedFn, err);
}

void tst_lupdate::good_data()
{
    QTest::addColumn<QString>("directory");

    QDir parsingDir(m_basePath + "good");
    QStringList dirs = parsingDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

#ifndef Q_OS_WIN
    dirs.removeAll(QLatin1String("backslashes"));
#endif

    foreach (const QString &dir, dirs)
        QTest::newRow(dir.toLocal8Bit()) << dir;
}

void tst_lupdate::good()
{
    QFETCH(QString, directory);

    QString dir = m_basePath + "good/" + directory;

    qDebug() << "Checking...";

    QString workDir = dir;
    QStringList generatedtsfiles(QLatin1String("project.ts"));
    QString lupdatecmd;

    QFile file(dir + "/lupdatecmd");
    if (file.exists()) {
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.fileName()));
        while (!file.atEnd()) {
            QByteArray cmdstring = file.readLine().simplified();
            if (cmdstring.startsWith('#'))
                continue;
            if (cmdstring.startsWith("lupdate")) {
                cmdstring.remove(0, 8);
                lupdatecmd.append(cmdstring);
                break;
            } else if (cmdstring.startsWith("TRANSLATION:")) {
                cmdstring.remove(0, 12);
                generatedtsfiles.clear();
                foreach (const QByteArray &s, cmdstring.split(' '))
                    if (!s.isEmpty())
                        generatedtsfiles << s;
            } else if (cmdstring.startsWith("cd ")) {
                cmdstring.remove(0, 3);
                workDir = QDir::cleanPath(dir + QLatin1Char('/') + cmdstring);
            }
        }
        file.close();
    }

    foreach (const QString &ts, generatedtsfiles) {
        QString genTs = workDir + QLatin1Char('/') + ts;
        QFile::remove(genTs);
        QString beforetsfile = dir + QLatin1Char('/') + ts + QLatin1String(".before");
        if (QFile::exists(beforetsfile))
            QVERIFY2(QFile::copy(beforetsfile, genTs), qPrintable(beforetsfile));
    }

    file.setFileName(workDir + QStringLiteral("/.qmake.cache"));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    if (lupdatecmd.isEmpty())
        lupdatecmd = QLatin1String("project.pro");
    lupdatecmd.prepend("-silent ");

    QProcess proc;
    proc.setWorkingDirectory(workDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    const QString command = QLatin1Char('"') + m_cmdLupdate + QLatin1String("\" ") + lupdatecmd;
    proc.start(command, QIODevice::ReadWrite | QIODevice::Text);
    QVERIFY2(proc.waitForStarted(), qPrintable(command + QLatin1String(" :") + proc.errorString()));
    QVERIFY2(proc.waitForFinished(30000), qPrintable(command));
    QByteArray output = proc.readAll();
    QVERIFY2(proc.exitStatus() == QProcess::NormalExit,
             "\"lupdate " + lupdatecmd.toLatin1() + "\" crashed\n" + output);
    QVERIFY2(!proc.exitCode(),
             "\"lupdate " + lupdatecmd.toLatin1() + "\" exited with code " +
             QByteArray::number(proc.exitCode()) + "\n" + output);

    // If the file expectedoutput.txt exists, compare the
    // console output with the content of that file
    QFile outfile(dir + "/expectedoutput.txt");
    if (outfile.exists()) {
        QStringList errslist = QString::fromLatin1(output).split(QLatin1Char('\n'));
        doCompare(errslist, outfile.fileName(), true);
        if (QTest::currentTestFailed())
            return;
    }

    foreach (const QString &ts, generatedtsfiles)
        doCompare(workDir + QLatin1Char('/') + ts,
                  dir + QLatin1Char('/') + ts + QLatin1String(".result"), false);
}

#if CHECK_SIMTEXTH
void tst_lupdate::simtexth()
{
    QFETCH(QString, one);
    QFETCH(QString, two);
    QFETCH(int, expected);

    int measured = getSimilarityScore(one, two.toLatin1());
    QCOMPARE(measured, expected);
}


void tst_lupdate::simtexth_data()
{
    using namespace QTest;

    addColumn<QString>("one");
    addColumn<QString>("two");
    addColumn<int>("expected");

    newRow("00") << "" << "" << 1024;
    newRow("01") << "a" << "a" << 1024;
    newRow("02") << "ab" << "ab" << 1024;
    newRow("03") << "abc" << "abc" << 1024;
    newRow("04") << "abcd" << "abcd" << 1024;
}
#endif

QTEST_MAIN(tst_lupdate)
#include "tst_lupdate.moc"
