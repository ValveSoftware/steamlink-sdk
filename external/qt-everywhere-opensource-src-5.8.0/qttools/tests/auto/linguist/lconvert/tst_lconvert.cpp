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

#include <QtTest/QtTest>
#include <QtCore/QFile>

class tst_lconvert : public QObject
{
    Q_OBJECT

public:
    tst_lconvert()
      : dataDir(QFINDTESTDATA("data/"))
      , lconvert(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/lconvert") {}

private slots:
    void initTestCase();
    void readverifies_data();
    void readverifies();
    void converts_data();
    void converts();
    void roundtrips_data();
    void roundtrips();
#if 0
    void chains_data();
    void chains();
#endif
    void merge();

private:
    void doWait(QProcess *cvt, int stage);
    void doCompare(QIODevice *actual, const QString &expectedFn);
    void verifyReadFail(const QString &fn);
    // args can be empty or have one element less than stations
    void convertChain(const QString &inFileName, const QString &outFileName,
            const QStringList &stations, const QList<QStringList> &args);
    void convertRoundtrip(const QString &fileName, const QStringList &stations,
            const QList<QStringList> &args);

    QString dataDir;
    QString lconvert;
};

void tst_lconvert::initTestCase()
{
    if (!QFile::exists(dataDir + QLatin1String("plural-1.po")))
        QProcess::execute(QLatin1String("perl"), QStringList() << dataDir + QLatin1String("makeplurals.pl") << dataDir + QLatin1String(""));
    QVERIFY(QFile::exists(dataDir + QLatin1String("plural-1.po")));
}

void tst_lconvert::doWait(QProcess *cvt, int stage)
{
    if (QTest::currentTestFailed()) {
        cvt->kill();
        cvt->waitForFinished();
    } else {
        QVERIFY2(cvt->waitForFinished(3000),
                 qPrintable(QString("Process %1 hung").arg(stage)));
        QVERIFY2(cvt->exitStatus() == QProcess::NormalExit,
                 qPrintable(QString("Process %1 crashed").arg(stage)));
        QVERIFY2(cvt->exitCode() == 0,
                 qPrintable(QString("Process %1 exited with status %2. Errors:\n%3")
                            .arg(stage).arg(cvt->exitCode())
                            .arg(QString::fromUtf8(cvt->readAllStandardError()))));
    }
}

void tst_lconvert::doCompare(QIODevice *actualDev, const QString &expectedFn)
{
    QList<QByteArray> actual = actualDev->readAll().split('\n');

    QFile file(expectedFn);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QList<QByteArray> expected = file.readAll().split('\n');

    int i = 0, ei = expected.size(), gi = actual.size();
    for (; ; i++) {
        if (i == gi) {
            if (i == ei)
                return;
            gi = 0;
            break;
        } else if (i == ei) {
            ei = 0;
            break;
        } else if (actual.at(i) != expected.at(i)) {
            while ((ei - 1) >= i && (gi - 1) >= i && actual.at(gi - 1) == expected.at(ei - 1))
                ei--, gi--;
            break;
        }
    }
    QByteArray diff;
    for (int j = qMax(0, i - 3); j < i; j++)
        diff += expected.at(j) + '\n';
    diff += "<<<<<<< got\n";
    for (int j = i; j < gi; j++) {
        diff += actual.at(j) + '\n';
        if (j >= i + 5) {
            diff += "...\n";
            break;
        }
    }
    diff += "=========\n";
    for (int j = i; j < ei; j++) {
        diff += expected.at(j) + '\n';
        if (j >= i + 5) {
            diff += "...\n";
            break;
        }
    }
    diff += ">>>>>>> expected\n";
    for (int j = ei; j < qMin(ei + 3, expected.size()); j++)
        diff += expected.at(j) + '\n';
    QFAIL(qPrintable("Output for " + expectedFn + " does not meet expectations:\n" + diff));
}

void tst_lconvert::verifyReadFail(const QString &fn)
{
    QProcess cvt;
    cvt.start(lconvert, QStringList() << (dataDir + fn));
    QVERIFY(cvt.waitForFinished(10000));
    QVERIFY(cvt.exitStatus() == QProcess::NormalExit);
    QVERIFY2(cvt.exitCode() == 2, "Accepted invalid input");
}

void tst_lconvert::convertChain(const QString &_inFileName, const QString &_outFileName,
    const QStringList &stations, const QList<QStringList> &argList)
{
    QList<QProcess *> cvts;

    QString fileName = dataDir + _inFileName;
    QString outFileName = dataDir + _outFileName;

    for (int i = 0; i < stations.size() - 1; i++) {
        QProcess *cvt = new QProcess(this);
        if (cvts.isEmpty())
            cvt->setStandardInputFile(fileName);
        else
            cvts.last()->setStandardOutputProcess(cvt);
        cvts.append(cvt);
    }
    for (int i = 0; i < stations.size() - 1; i++) {
        QStringList args;
        if (!argList.isEmpty())
            args += argList[i];
        args << "-if" << stations[i] << "-i" << "-" << "-of" << stations[i + 1];
        cvts.at(i)->start(lconvert, args, QIODevice::ReadWrite | QIODevice::Text);
    }
    int st = 0;
    foreach (QProcess *cvt, cvts)
        doWait(cvt, ++st);

    if (!QTest::currentTestFailed())
        doCompare(cvts.last(), outFileName);

    qDeleteAll(cvts);
}

void tst_lconvert::convertRoundtrip(const QString &_fileName, const QStringList &stations,
    const QList<QStringList> &argList)
{
    convertChain(_fileName, _fileName, stations, argList);
}

void tst_lconvert::readverifies_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("format");

    QTest::newRow("ts") << "test20.ts" << "ts";
    QTest::newRow("empty comment") << "test-empty-comment.po" << "po";
    QTest::newRow("translator comment") << "test-translator-comment.po" << "po";
    QTest::newRow("developer comment") << "test-developer-comment.po" << "po";
    QTest::newRow("kde context") << "test-kde-ctxt.po" << "po";
    QTest::newRow("kde fuzzy") << "test-kde-fuzzy.po" << "po";
    QTest::newRow("kde plurals") << "test-kde-plurals.po" << "po";
    QTest::newRow("kde multiline") << "test-kde-multiline.po" << "po";
    QTest::newRow("po linewrapping") << "wrapping.po" << "po";
    QTest::newRow("relative locations") << "relative.ts" << "ts";
    QTest::newRow("message ids") << "msgid.ts" << "ts";
    QTest::newRow("length variants") << "variants.ts" << "ts";
    QTest::newRow("qph") << "phrasebook.qph" << "qph";
}

void tst_lconvert::readverifies()
{
    QFETCH(QString, fileName);
    QFETCH(QString, format);

    convertRoundtrip(fileName, QStringList() << format << format, QList<QStringList>());
}

void tst_lconvert::converts_data()
{
    QTest::addColumn<QString>("inFileName");
    QTest::addColumn<QString>("outFileName");
    QTest::addColumn<QString>("format");

    QTest::newRow("broken utf8") << "test-broken-utf8.po" << "test-broken-utf8.po.out" << "po";
    QTest::newRow("line joins") << "test-slurp.po" << "test-slurp.po.out" << "po";
    QTest::newRow("escapes") << "test-escapes.po" << "test-escapes.po.out" << "po";
}

void tst_lconvert::converts()
{
    QFETCH(QString, inFileName);
    QFETCH(QString, outFileName);
    QFETCH(QString, format);

    QString outFileNameFq = dataDir + outFileName;

    QProcess cvt;
    cvt.start(lconvert,
              QStringList() << "-i" << (dataDir + inFileName) << "-of" << format,
              QIODevice::ReadWrite | QIODevice::Text);
    doWait(&cvt, 0);
    if (QTest::currentTestFailed())
        return;

    doCompare(&cvt, outFileNameFq);
}

Q_DECLARE_METATYPE(QList<QStringList>);

#if 0
void tst_lconvert::chains_data()
{
    QTest::addColumn<QString>("inFileName");
    QTest::addColumn<QString>("outFileName");
    QTest::addColumn<QStringList>("stations");
    QTest::addColumn<QList<QStringList> >("args");

}

void tst_lconvert::chains()
{
    QFETCH(QString, inFileName);
    QFETCH(QString, outFileName);
    QFETCH(QStringList, stations);
    QFETCH(QList<QStringList>, args);

    convertChain(inFileName, outFileName, stations, args);
}
#endif

void tst_lconvert::roundtrips_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QStringList>("stations");
    QTest::addColumn<QList<QStringList> >("args");

    QStringList poTsPo; poTsPo << "po" << "ts" << "po";
    QStringList poXlfPo; poXlfPo << "po" << "xlf" << "po";
    QStringList tsPoTs; tsPoTs << "ts" << "po" << "ts";
    QStringList tsXlfTs; tsXlfTs << "ts" << "xlf" << "ts";
    QStringList tsQmTs; tsQmTs << "ts" << "qm" << "ts";

    QList<QStringList> noArgs;
    QList<QStringList> filterPoArgs; filterPoArgs << QStringList() << (QStringList() << "-drop-tag" << "po:*");
    QList<QStringList> outDeArgs; outDeArgs << QStringList() << (QStringList() << "-target-language" << "de");
    QList<QStringList> outCnArgs; outCnArgs << QStringList() << (QStringList() << "-target-language" << "cn");

    QTest::newRow("po-ts-po (translator comment)") << "test-translator-comment.po" << poTsPo << noArgs;
    QTest::newRow("po-xliff-po (translator comment)") << "test-translator-comment.po" << poXlfPo << noArgs;
    QTest::newRow("po-ts-po (developer comment)") << "test-developer-comment.po" << poTsPo << noArgs;
    QTest::newRow("po-xliff-po (developer comment)") << "test-developer-comment.po" << poXlfPo << noArgs;

    QTest::newRow("ts20-po-ts20") << "test20.ts" << tsPoTs << filterPoArgs;
    QTest::newRow("po-ts-po (de)") << "test1-de.po" << poTsPo << noArgs;
    QTest::newRow("po-ts-po (cn)") << "test1-cn.po" << poTsPo << noArgs;
    QTest::newRow("po-xliff-po (de)") << "test1-de.po" << poXlfPo << noArgs;
    QTest::newRow("po-xliff-po (cn)") << "test1-cn.po" << poXlfPo << noArgs;

    QTest::newRow("po-ts-po (singular)") << "singular.po" << poTsPo << noArgs;
    QTest::newRow("po-ts-po (plural-1)") << "plural-1.po" << poTsPo << noArgs;
    QTest::newRow("po-ts-po (plural-2)") << "plural-2.po" << poTsPo << noArgs;
    QTest::newRow("po-ts-po (plural-3)") << "plural-3.po" << poTsPo << noArgs;
    QTest::newRow("po-xliff-po (singular)") << "singular.po" << poXlfPo << noArgs;
    QTest::newRow("po-xliff-po (plural-1)") << "plural-1.po" << poXlfPo << noArgs;
    QTest::newRow("po-xliff-po (plural-2)") << "plural-2.po" << poXlfPo << noArgs;
    QTest::newRow("po-xliff-po (plural-3)") << "plural-3.po" << poXlfPo << noArgs;

    QTest::newRow("po-ts-po (references)") << "test-refs.po" << poTsPo << noArgs;

    QTest::newRow("ts-qm-ts (plurals-de)") << "plurals-de.ts" << tsQmTs << outDeArgs;
    QTest::newRow("ts-qm-ts (plurals-cn)") << "plurals-cn.ts" << tsQmTs << outCnArgs;
    QTest::newRow("ts-qm-ts (variants)") << "variants.ts" << tsQmTs << outDeArgs;
    QTest::newRow("ts-po-ts (msgid)") << "msgid.ts" << tsPoTs << noArgs;
    QTest::newRow("ts-xliff-ts (msgid)") << "msgid.ts" << tsXlfTs << noArgs;

    QTest::newRow("ts-po-ts (endless loop)") << "endless-po-loop.ts" << tsPoTs << noArgs;
}

void tst_lconvert::roundtrips()
{
    QFETCH(QString, fileName);
    QFETCH(QStringList, stations);
    QFETCH(QList<QStringList>, args);

    convertRoundtrip(fileName, stations, args);
}

void tst_lconvert::merge()
{
    QProcess cvt;
    QStringList args;
    args << (dataDir + "idxmerge.ts") << (dataDir + "idxmerge-add.ts");
    cvt.start(lconvert, args, QIODevice::ReadWrite | QIODevice::Text);
    doWait(&cvt, 1);
    if (!QTest::currentTestFailed())
        doCompare(&cvt, dataDir + "idxmerge.ts.out");
}

QTEST_APPLESS_MAIN(tst_lconvert)

#include "tst_lconvert.moc"
