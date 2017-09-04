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
#include <QtCore/QString>
#include <QSound>
#include <QSoundEffect>

class tst_QSound : public QObject
{
   Q_OBJECT

public:
   tst_QSound( QObject* parent=0) : QObject(parent) {}

private slots:
   void initTestCase();
   void cleanupTestCase();
   void testLooping();
   void testPlay();
   void testStop();

   void testStaticPlay();

private:
    QSound* sound;
};


void tst_QSound::initTestCase()
{
    sound = 0;
    // Only perform tests if audio device exists
    QStringList mimeTypes = QSoundEffect::supportedMimeTypes();
    if (mimeTypes.empty())
        QSKIP("No audio devices available");

    const QString testFileName = QStringLiteral("test.wav");
    const QString fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));
    sound = new QSound(fullPath, this);

    QVERIFY(!sound->fileName().isEmpty());
    QCOMPARE(sound->loops(),1);
}

void tst_QSound::cleanupTestCase()
{
    if (sound)
    {
        delete sound;
        sound = NULL;
    }
}

void tst_QSound::testLooping()
{
    sound->setLoops(5);
    QCOMPARE(sound->loops(),5);

    sound->play();
    QVERIFY(!sound->isFinished());

    // test.wav is about 200ms, wait until it has finished playing 5 times
    QTest::qWait(3000);

    QVERIFY(sound->isFinished());
    QCOMPARE(sound->loopsRemaining(),0);
}

void tst_QSound::testPlay()
{
    sound->setLoops(1);
    sound->play();
    QVERIFY(!sound->isFinished());
    QTest::qWait(1000);
    QVERIFY(sound->isFinished());
}

void tst_QSound::testStop()
{
    sound->setLoops(10);
    sound->play();
    QVERIFY(!sound->isFinished());
    QTest::qWait(1000);
    sound->stop();
    QTest::qWait(1000);
    QVERIFY(sound->isFinished());
}

void tst_QSound::testStaticPlay()
{
    // Check that you hear sound with static play also.
    const QString testFileName = QStringLiteral("test.wav");
    const QString fullPath = QFINDTESTDATA(testFileName);
    QVERIFY2(!fullPath.isEmpty(), qPrintable(QStringLiteral("Unable to locate ") + testFileName));

    QSound::play(fullPath);

    QTest::qWait(1000);
}

QTEST_MAIN(tst_QSound);
#include "tst_qsound.moc"
